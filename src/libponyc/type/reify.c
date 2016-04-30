#include "reify.h"
#include "subtype.h"
#include "viewpoint.h"
#include "assemble.h"
#include "alias.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include "../evaluate/evaluate.h"
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

  ast_t* constraint = ast_childidx(typeparam, 1);
  assert(ast_id(constraint) != TK_TYPEPARAMREF);
  // Values can be constrained by other type parameters
  // we want to use the reified version of that parameter

  ast_settype(*astp, constraint);
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

    // FIXME: failures can occur here
    case TK_CONSTANT:
      assert(expr_constant(astp));
      break;

    default: {}
  }
}

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors)
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
      ast_error(typeargs, "too many type arguments");
      ast_error(typeparams, "definition is here");
    }

    return false;
  }

  // Pick up default type arguments if they exist.
  ast_setid(typeargs, TK_TYPEARGS);
  ast_t* typeparam = ast_childidx(typeparams, arg_count);

  while(typeparam != NULL)
  {
    ast_t* defarg = ast_childidx(typeparam, 2);

    if(ast_id(defarg) == TK_NONE)
      break;

    ast_append(typeargs, defarg);
    typeparam = ast_sibling(typeparam);
  }

  if(typeparam != NULL)
  {
    if(errors)
    {
      ast_error(typeargs, "not enough type arguments");
      ast_error(typeparams, "definition is here");
    }

    return false;
  }

  return true;
}

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs)
{
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

  // We may need to reify some constraints of the typeparameters as well
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
    if(ast_id(typearg) == TK_TYPEPARAMREF)
    {
      ast_t* def = (ast_t*)ast_data(typearg);

      if(def == typeparam)
      {
        typeparam = ast_sibling(typeparam);
        typearg = ast_sibling(typearg);
        continue;
      }
    }

    token_id typeparam_id = ast_id(typeparam);
    if(!compatible_argument(typeparam_id, ast_id(typearg)))
    {
      bool is_typeparam = typeparam_id == TK_TYPEPARAM;
      ast_error(orig, "invalid parameterisation");
      ast_error(typeparam, "expected %s argument", is_typeparam ? "type" : "value");
      ast_error(typearg, "provided %s argument", is_typeparam ? "value" : "type");
      return false;
    }

    // Reify the constraint.
    ast_t* constraint = ast_childidx(typeparam, 1);
    ast_t* bind_constraint = bind_type(constraint);
    ast_t* r_constraint = reify(bind_constraint, typeparams, typeargs);

    if(bind_constraint != r_constraint)
      ast_free_unattached(bind_constraint);
    errorframe_t info = NULL;

    if (ast_id(typearg) == TK_VALUEFORMALARG) {
      ast_t* value = ast_child(typearg);
      if(!coerce_literals(&value, r_constraint, opt))
        return false;

      ast_t* value_type = ast_type(value);
      if(value_type == NULL)
        return false;

      if (!is_subtype(value_type, r_constraint, report_errors ? &info : NULL)) {
        if(report_errors)
        {
          errorframe_t frame = NULL;
          ast_error_frame(&frame, orig,
            "value argument type is outside its constraint");
          ast_error_frame(&frame, value,
            "argument type: %s", ast_print_type(ast_type(value)));
          ast_error_frame(&frame, typeparam,
            "constraint: %s", ast_print_type(r_constraint));
          errorframe_append(&frame, &info);
          errorframe_report(&frame);
        }

        ast_free_unattached(r_constraint);
        return false;
      }
    }
    // A bound type must be a subtype of the constraint.
    else if(!is_subtype(typearg, r_constraint, report_errors ? &info : NULL))
    {
      if(report_errors)
      {
        errorframe_t frame = NULL;
        ast_error_frame(&frame, orig,
          "type argument is outside its constraint");
        ast_error_frame(&frame, typearg,
          "argument: %s", ast_print_type(typearg));
        ast_error_frame(&frame, typeparam,
          "constraint: %s", ast_print_type(r_constraint));
        errorframe_append(&frame, &info);
        errorframe_report(&frame);
      }

      ast_free_unattached(r_constraint);
      return false;
    }

    ast_free_unattached(r_constraint);

    // A constructable constraint can only be fulfilled by a concrete typearg.
    if(is_constructable(constraint) && !is_concrete(typearg))
    {
      if(report_errors)
      {
        ast_error(orig, "a constructable constraint can only be fulfilled "
          "by a concrete type argument");
        ast_error(typearg, "argument: %s", ast_print_type(typearg));
        ast_error(typeparam, "constraint: %s", ast_print_type(constraint));
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
