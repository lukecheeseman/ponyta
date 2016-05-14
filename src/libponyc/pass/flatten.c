#include "flatten.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include "../type/compattype.h"
#include "../type/subtype.h"
#include "../type/typeparam.h"
#include <assert.h>

static ast_result_t flatten_union(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  // If there are more than 2 children, this has already been flattened.
  if(ast_childcount(ast) > 2)
    return AST_OK;

  AST_EXTRACT_CHILDREN(ast, left, right);

  ast_t* r_ast = type_union(opt, left, right);
  ast_replace(astp, r_ast);
  ast_free_unattached(left);
  ast_free_unattached(right);

  return AST_OK;
}

static void flatten_isect_element(ast_t* type, ast_t* elem)
{
  if(ast_id(elem) != TK_ISECTTYPE)
  {
    ast_append(type, elem);
    return;
  }

  ast_t* child = ast_child(elem);

  while(child != NULL)
  {
    ast_append(type, child);
    child = ast_sibling(child);
  }

  ast_free_unattached(elem);
}

static ast_result_t flatten_isect(pass_opt_t* opt, ast_t* ast)
{
  // Flatten intersections without testing subtyping. This is to preserve any
  // type guarantees that an element in the intersection might make.
  // If there are more than 2 children, this has already been flattened.
  if(ast_childcount(ast) > 2)
    return AST_OK;

  AST_EXTRACT_CHILDREN(ast, left, right);

  if((opt->check.frame->constraint == NULL) &&
    (opt->check.frame->provides == NULL) &&
    !is_compat_type(left, right))
  {
    ast_add(ast, right);
    ast_add(ast, left);

    ast_error(opt->check.errors, ast,
      "intersection types cannot include reference capabilities that are not "
      "locally compatible");
    return AST_ERROR;
  }

  flatten_isect_element(ast, left);
  flatten_isect_element(ast, right);

  return AST_OK;
}

ast_result_t flatten_typeparamref(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, cap, eph);

  if(ast_id(cap) != TK_NONE)
  {
    ast_error(opt->check.errors, cap,
      "can't specify a capability on a type parameter");
    return AST_ERROR;
  }

  typeparam_set_cap(ast);
  return AST_OK;
}

static ast_result_t flatten_noconstraint(pass_opt_t* opt, ast_t* ast)
{
  if(opt->check.frame->constraint != NULL)
  {
    switch(ast_id(ast))
    {
      case TK_TUPLETYPE:
        ast_error(opt->check.errors, ast,
          "tuple types can't be used as constraints");
        return AST_ERROR;

      case TK_ARROW:
        if(opt->check.frame->method == NULL)
        {
          ast_error(opt->check.errors, ast,
            "arrow types can't be used as type constraints");
          return AST_ERROR;
        }
        break;

      default: {}
    }
  }

  return AST_OK;
}

static ast_result_t flatten_sendable_params(pass_opt_t* opt, ast_t* params)
{
  ast_t* param = ast_child(params);
  ast_result_t r = AST_OK;

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, type, def);

    if(!sendable(type))
    {
      ast_error(opt->check.errors, type,
        "this parameter must be sendable (iso, val or tag)");
      r = AST_ERROR;
    }

    param = ast_sibling(param);
  }

  return r;
}

static ast_result_t flatten_constructor(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_VAL:
      return flatten_sendable_params(opt, params);

    default: {}
  }

  return AST_OK;
}

static ast_result_t flatten_async(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  return flatten_sendable_params(opt, params);
}

// Process the given provides type
static bool flatten_provided_type(pass_opt_t* opt, ast_t* provides_type,
  ast_t* error_at, ast_t* list_parent, ast_t** list_end)
{
  assert(error_at != NULL);
  assert(provides_type != NULL);
  assert(list_parent != NULL);
  assert(list_end != NULL);

  switch(ast_id(provides_type))
  {
    case TK_PROVIDES:
    case TK_ISECTTYPE:
      // Flatten all children
      for(ast_t* p = ast_child(provides_type); p != NULL; p = ast_sibling(p))
      {
        if(!flatten_provided_type(opt, p, error_at, list_parent, list_end))
          return false;
      }

      return true;

    case TK_NOMINAL:
    {
      // Check type is a trait or interface
      ast_t* def = (ast_t*)ast_data(provides_type);
      assert(def != NULL);

      if(ast_id(def) != TK_TRAIT && ast_id(def) != TK_INTERFACE)
      {
        ast_error(opt->check.errors, error_at,
          "can only provide traits and interfaces");
        ast_error_continue(opt->check.errors, provides_type,
          "invalid type here");
        return false;
      }

      // Add type to new provides list
      ast_list_append(list_parent, list_end, provides_type);
      ast_setdata(*list_end, ast_data(provides_type));

      return true;
    }

    default:
      ast_error(opt->check.errors, error_at,
        "provides type may only be an intersect of traits and interfaces");
      ast_error_continue(opt->check.errors, provides_type, "invalid type here");
      return false;
  }
}

// Flatten a provides type into a list, checking all types are traits or
// interfaces
static ast_result_t flatten_provides_list(pass_opt_t* opt, ast_t* provider,
  int index)
{
  assert(provider != NULL);

  ast_t* provides = ast_childidx(provider, index);

  if(ast_id(provides) == TK_NONE)
    return AST_OK;

  ast_t* list = ast_from(provides, TK_PROVIDES);
  ast_t* list_end = NULL;

  if(!flatten_provided_type(opt, provides, provider, list, &list_end))
  {
    ast_free(list);
    return AST_ERROR;
  }

  ast_replace(&provides, list);
  return AST_OK;
}

ast_result_t pass_flatten(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
      return flatten_union(options, astp);

    case TK_ISECTTYPE:
      return flatten_isect(options, ast);

    case TK_NEW:
    {
      switch(ast_id(options->check.frame->type))
      {
        case TK_CLASS:
          return flatten_constructor(options, ast);

        case TK_ACTOR:
          return flatten_async(options, ast);

        default: {}
      }
      break;
    }

    case TK_BE:
      return flatten_async(options, ast);

    case TK_TUPLETYPE:
    case TK_ARROW:
      return flatten_noconstraint(options, ast);

    case TK_TYPEPARAMREF:
      return flatten_typeparamref(options, ast);

    case TK_FVAR:
    case TK_FLET:
      return flatten_provides_list(options, ast, 3);

    case TK_EMBED:
    {
      // An embedded field must have a known, class type.
      AST_GET_CHILDREN(ast, id, type, init);
      bool ok = true;

      if(ast_id(type) != TK_NOMINAL)
        ok = false;

      ast_t* def = (ast_t*)ast_data(type);

      if(def == NULL)
      {
        ok = false;
      } else {
        switch(ast_id(def))
        {
          case TK_STRUCT:
          case TK_CLASS:
            break;

          default:
            ok = false;
            break;
        }
      }

      if(!ok)
      {
        ast_error(options->check.errors, type,
          "embedded fields must be classes or structs");
        return AST_ERROR;
      }

      if(cap_single(type) == TK_TAG)
      {
        ast_error(options->check.errors, type, "embedded fields cannot be tag");
        return AST_ERROR;
      }

      return flatten_provides_list(options, ast, 3);
    }

    case TK_ACTOR:
    case TK_CLASS:
    case TK_STRUCT:
    case TK_PRIMITIVE:
    case TK_TRAIT:
    case TK_INTERFACE:
      return flatten_provides_list(options, ast, 3);

    case TK_OBJECT:
      return flatten_provides_list(options, ast, 0);

    default: {}
  }

  return AST_OK;
}
