#include "reify.h"
#include "subtype.h"
#include "viewpoint.h"
#include "assemble.h"
#include "alias.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../evaluate/evaluate.h"
#include "../pass/expr.h"
#include <assert.h>
#include <string.h>

static void reify_typeparamref(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_TYPEPARAMREF);

  ast_t* ref_def = (ast_t*)ast_data(ast);
  ast_t* param_def = (ast_t*)ast_data(typeparam);

  assert(ref_def != NULL);
  assert(param_def != NULL);

  if(ref_def != param_def)
    return;

  // Keep ephemerality.
  switch(ast_id(ast_childidx(ast, 2)))
  {
    case TK_EPHEMERAL:
      typearg = consume_type(typearg, TK_NONE);
      break;

    case TK_NONE:
      break;

    case TK_BORROWED:
      typearg = alias(typearg);
      break;

    default:
      assert(0);
  }

  ast_replace(astp, typearg);
}

static void reify_valueformalparamref(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_VALUEFORMALPARAMREF);
  ast_t* ref_name = ast_child(ast);
  ast_t* param_name = ast_child(typeparam);

  if(strcmp(ast_name(ref_name), ast_name(param_name)))
    return;

  if(ast_checkreified(ast))
    return;

  ast_replace(astp, ast_child(typearg));
  ast_setreified(*astp);
}

static void reify_arrow(ast_t** astp)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_ARROW);
  AST_GET_CHILDREN(ast, left, right);

  ast_t* r_left = left;
  ast_t* r_right = right;

  if(ast_id(left) == TK_ARROW)
  {
    AST_GET_CHILDREN(left, l_left, l_right);
    r_left = l_left;
    r_right = viewpoint_type(l_right, right);
  }

  ast_t* r_type = viewpoint_type(r_left, r_right);
  ast_replace(astp, r_type);
}

static void reify_one(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    reify_one(&child, typeparam, typearg);
    child = ast_sibling(child);
  }

  ast_t* type = ast_type(ast);

  if(type != NULL)
    reify_one(&type, typeparam, typearg);

  switch(ast_id(ast))
  {
    case TK_TYPEPARAMREF:
      reify_typeparamref(astp, typeparam, typearg);
      break;

    case TK_VALUEFORMALPARAMREF:
      reify_valueformalparamref(astp, typeparam, typearg);
      break;

    case TK_ARROW:
      reify_arrow(astp);
      break;

    default: {}
  }
}

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors,
  pass_opt_t* opt)
{
  assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  size_t param_count = ast_childcount(typeparams);
  size_t arg_count = ast_childcount(typeargs);

  if(param_count == arg_count)
    return true;

  if(param_count < arg_count)
  {
    if(errors)
    {
      ast_error(opt->check.errors, typeargs, "too many type arguments");
      ast_error_continue(opt->check.errors, typeparams, "definition is here");
    }

    return false;
  }

  // Pick up default type arguments if they exist.
  ast_setid(typeargs, TK_TYPEARGS);
  ast_t* typeparam = ast_childidx(typeparams, arg_count);

  while(typeparam != NULL)
  {
    ast_t* defarg = ast_childidx(typeparam, 3);

    if(ast_id(defarg) == TK_NONE)
      break;

    ast_append(typeargs, defarg);
    typeparam = ast_sibling(typeparam);
  }

  if(typeparam != NULL)
  {
    if(errors)
    {
      ast_error(opt->check.errors, typeargs, "not enough type arguments");
      ast_error_continue(opt->check.errors, typeparams, "definition is here");
    }

    return false;
  }

  return true;
}

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt)
{
  (void)opt;
  assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  // Duplicate the node.
  ast_t* r_ast = ast_dup(ast);

  // We may need to reify some constraints of the typeparameters when we need to
  // ensure that a value argument satisfies the constraints of another type
  // parameter.
  typeparams = ast_dup(typeparams);

  // Iterate pairwise through the typeparams and typeargs.
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    reify_one(&r_ast, typeparam, typearg);
    reify_one(&typeparams, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  assert(typeparam == NULL);
  assert(typearg == NULL);

  ast_free(typeparams);

  return r_ast;
}

static bool compatible_argument(token_id typeparam_id, token_id typearg_id)
{
  bool is_value_argument =
    typearg_id == TK_VALUEFORMALPARAMREF || typearg_id == TK_VALUEFORMALARG;
  return (typeparam_id == TK_VALUEFORMALPARAM && is_value_argument) ||
         (typeparam_id == TK_TYPEPARAM && !is_value_argument);
}

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors, pass_opt_t* opt)
{
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while(typeparam != NULL)
  {
    switch(ast_id(typearg))
    {
      case TK_NOMINAL:
      {
        ast_t* def = (ast_t*)ast_data(typearg);

        if(ast_id(def) == TK_STRUCT)
        {
          if(report_errors)
          {
            ast_error(opt->check.errors, typearg,
              "a struct cannot be used as a type argument");
          }

          return false;
        }
        break;
      }

      case TK_TYPEPARAMREF:
      {
        ast_t* def = (ast_t*)ast_data(typearg);

        if(def == typeparam)
        {
          typeparam = ast_sibling(typeparam);
          typearg = ast_sibling(typearg);
          continue;
        }
        break;
      }

      default: {}
    }

    token_id typeparam_id = ast_id(typeparam);
    if(!compatible_argument(typeparam_id, ast_id(typearg)))
    {
      bool is_typeparam = typeparam_id == TK_TYPEPARAM;
      ast_error(opt->check.errors, orig,
        "invalid parameterisation");
      ast_error_continue(opt->check.errors, typeparam,
        "expected %s argument", is_typeparam ? "type" : "value");
      ast_error_continue(opt->check.errors, typearg,
        "provided %s argument", is_typeparam ? "value" : "type");
      return false;
    }

    // type check the typeparam in case it is a value typeparameter
    if(ast_visit(&typeparam, NULL, pass_expr, opt, PASS_EXPR) != AST_OK)
      return false;

    // Reify the constraint.
    ast_t* constraint = ast_childidx(typeparam, 1);
    ast_t* bind_constraint = bind_type(constraint);
    ast_t* r_constraint = reify(bind_constraint, typeparams, typeargs, opt);

    if(bind_constraint != r_constraint)
      ast_free_unattached(bind_constraint);
    errorframe_t info = NULL;

    if (ast_id(typearg) == TK_VALUEFORMALARG) {
      ast_t* value = ast_child(typearg);
      if(!coerce_literals(&value, r_constraint, opt))
        return false;

      ast_t* value_type = ast_type(value);
      if(is_typecheck_error(value_type))
        return false;

      if (!is_subtype(value_type, r_constraint, report_errors ? &info : NULL, opt)) {
        if(report_errors)
        {
          ast_error(opt->check.errors, orig,
            "value argument type is outside its constraint");
          ast_error_continue(opt->check.errors, value,
            "argument type: %s", ast_print_type(ast_type(value)));
          ast_error_continue(opt->check.errors, typeparam,
            "constraint: %s", ast_print_type(r_constraint));
        }

        ast_free_unattached(r_constraint);
        return false;
      }
    }
    // A bound type must be a subtype of the constraint.
    else if(!is_subtype(typearg, r_constraint, report_errors ? &info : NULL, opt))
    {
      if(report_errors)
      {
        ast_error(opt->check.errors, orig,
          "type argument is outside its constraint");
        ast_error_continue(opt->check.errors, typearg,
          "argument: %s", ast_print_type(typearg));
        ast_error_continue(opt->check.errors, typeparam,
          "constraint: %s", ast_print_type(r_constraint));
      }

      ast_free_unattached(r_constraint);
      return false;
    }

    ast_free_unattached(r_constraint);

    // TODO: this will probably (definetly) not work completely for the same
    // reason we can't evaluate expressions in type checking. May have to be
    // moved later
    ast_t* value_constraint = ast_childidx(typeparam, 2);
    if(ast_id(value_constraint) != TK_NONE)
    {
      ast_t* r_vconstraint = reify(value_constraint, typeparams, typeargs, opt);
      r_vconstraint = ast_child(r_vconstraint);
      if(!contains_valueparamref(r_vconstraint))
      {
        evaluate_expression(opt, &r_vconstraint);
        if(r_vconstraint == NULL)
          return false;

        if(ast_id(r_vconstraint) == TK_FALSE)
        {
          if(report_errors)
          {
            ast_error(opt->check.errors, orig,
              "type argument is outside its constraint");
            ast_error_continue(opt->check.errors, typearg,
              "argument: %s", ast_print_expr(ast_child(typearg)));
            ast_error_continue(opt->check.errors, value_constraint,
              "constraint: %s", ast_print_expr(ast_child(value_constraint)));
          }

          ast_free_unattached(r_vconstraint);
          return false;
        }

        assert(ast_id(r_vconstraint) == TK_TRUE);
      }
      ast_free_unattached(r_vconstraint);
    }

    // A constructable constraint can only be fulfilled by a concrete typearg.
    if(is_constructable(constraint) && !is_concrete(typearg))
    {
      if(report_errors)
      {
        ast_error(opt->check.errors, orig, "a constructable constraint can "
          "only be fulfilled by a concrete type argument");
        ast_error_continue(opt->check.errors, typearg, "argument: %s",
          ast_print_type(typearg));
        ast_error_continue(opt->check.errors, typeparam, "constraint: %s",
          ast_print_type(constraint));
      }

      return false;
    }

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  assert(typeparam == NULL);
  assert(typearg == NULL);
  return true;
}
