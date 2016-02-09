#include "reify.h"
#include "subtype.h"
#include "viewpoint.h"
#include "assemble.h"
#include "alias.h"
#include "../ast/token.h"
#include "../expr/literal.h"
#include <assert.h>

static void reify_typeparamref(ast_t** astp, ast_t* typeparam, ast_t* typearg)
{
  ast_t* ast = *astp;
  assert(ast_id(ast) == TK_TYPEPARAMREF);
  ast_t* ref_name = ast_child(ast);
  ast_t* param_name = ast_child(typeparam);

  if(ast_name(ref_name) != ast_name(param_name))
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

    case TK_ARROW:
      reify_arrow(astp);
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

  // Iterate pairwise through the typeparams and typeargs.
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    reify_one(&r_ast, typeparam, typearg);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  assert(typeparam == NULL);
  assert(typearg == NULL);
  return r_ast;
}

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors)
{
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while(typeparam != NULL)
  {
    // Reify the constraint.
    ast_t* constraint = ast_childidx(typeparam, 1);
    ast_t* bind_constraint = bind_type(constraint);
    ast_t* r_constraint = reify(bind_constraint, typeparams, typeargs);

    if(bind_constraint != r_constraint)
      ast_free_unattached(bind_constraint);
    errorframe_t info = NULL;
    if (ast_id(typearg) == TK_VALUEFORMALARG) {
      ast_t *value = ast_child(typearg);
      pass_opt_t opts;
      if(!coerce_literals(&value, r_constraint, &opts))
        return false;

      if (!is_subtype(ast_type(value), r_constraint, report_errors ? &info : NULL)) {
        if(report_errors)
        {
          errorframe_t frame = NULL;
          ast_error_frame(&frame, orig, "value argument type is outside its constraint");
          ast_error_frame(&frame, value, "argument type: %s", ast_print_type(ast_type(value)));
          ast_error_frame(&frame, typeparam, "constraint: %s", ast_print_type(r_constraint));
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
