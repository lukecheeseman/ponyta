#include "lookup.h"
#include "assemble.h"
#include "cap.h"
#include "reify.h"
#include "viewpoint.h"
#include "subtype.h"
#include "../ast/token.h"
#include "../ast/id.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../expr/literal.h"
#include <string.h>
#include <assert.h>

static ast_t* lookup_base(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors);

static ast_t* lookup_nominal(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  assert(ast_id(type) == TK_NOMINAL);
  typecheck_t* t = &opt->check;

  ast_t* def = (ast_t*)ast_data(type);
  AST_GET_CHILDREN(def, type_id, typeparams);
  const char* type_name = ast_name(type_id);

  if(is_name_private(type_name) && (from != NULL) && (opt != NULL))
  {
    if(ast_nearest(def, TK_PACKAGE) != t->frame->package)
    {
      if(errors)
      {
        ast_error(opt->check.errors, from, "can't lookup fields or methods "
          "on private types from other packages");
      }

      return NULL;
    }
  }

  ast_t* find = ast_get(def, name, NULL);

  if(find != NULL)
  {
    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // Typecheck default args immediately.
        if(opt != NULL)
        {
          AST_GET_CHILDREN(find, cap, id, typeparams, params);
          ast_t* param = ast_child(params);

          while(param != NULL)
          {
            AST_GET_CHILDREN(param, name, type, def_arg);

            if((ast_id(def_arg) != TK_NONE) && (ast_type(def_arg) == NULL))
            {
              ast_settype(def_arg, ast_from(def_arg, TK_INFERTYPE));

              if(ast_visit_scope(&param, NULL, pass_expr, opt,
                PASS_EXPR) != AST_OK)
                return false;

              def_arg = ast_childidx(param, 2);

              if(!coerce_literals(&def_arg, type, opt))
                return false;
            }

            param = ast_sibling(param);
          }
        }
        break;
      }

      default:
        find = NULL;
    }
  }

  if(find == NULL)
  {
    if(errors)
      ast_error(opt->check.errors, from, "couldn't find '%s' in '%s'", name,
        type_name);

    return NULL;
  }

  if(is_name_private(name) && (from != NULL) && (opt != NULL))
  {
    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        if(t->frame->type != def)
        {
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup private fields from outside the type");
          }

          return NULL;
        }
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        if(ast_nearest(def, TK_PACKAGE) != t->frame->package)
        {
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup private methods from outside the package");
          }

          return NULL;
        }
        break;
      }

      default:
        assert(0);
        return NULL;
    }

    if(!strcmp(name, "_final"))
    {
      switch(ast_id(find))
      {
        case TK_NEW:
        case TK_BE:
        case TK_FUN:
          if(errors)
            ast_error(opt->check.errors, from,
              "can't lookup a _final function");

          return NULL;

        default: {}
      }
    }
  }

  ast_t* typeargs = ast_childidx(type, 2);
  ast_t* r_find = viewpoint_replacethis(find, orig);
  ast_t* rr_find = reify(r_find, typeparams, typeargs, opt);
  ast_free_unattached(r_find);
  return rr_find;
}

static ast_t* lookup_typeparam(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  ast_t* def = (ast_t*)ast_data(type);
  ast_t* constraint = ast_childidx(def, 1);
  ast_t* constraint_def = (ast_t*)ast_data(constraint);

  if(def == constraint_def)
  {
    if(errors)
    {
      ast_t* type_id = ast_child(def);
      const char* type_name = ast_name(type_id);
      ast_error(opt->check.errors, from, "couldn't find '%s' in '%s'",
        name, type_name);
    }

    return NULL;
  }

  // Lookup on the constraint instead.
  return lookup_base(opt, from, orig, constraint, name, errors);
}

static ast_t* lookup_union(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name, bool errors)
{
  ast_t* child = ast_child(type);
  ast_t* result = NULL;
  bool ok = true;

  while(child != NULL)
  {
    ast_t* r = lookup_base(opt, from, child, child, name, errors);

    if(r == NULL)
    {
      // All possible types in the union must have this.
      if(errors)
      {
        ast_error(opt->check.errors, from, "couldn't find %s in %s",
          name, ast_print_type(child));
      }

      ok = false;
    } else {
      switch(ast_id(r))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED:
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup field %s in %s in a union type",
              name, ast_print_type(child));
          }

          ok = false;
          break;

        default:
        {
          errorframe_t frame = NULL;
          errorframe_t* pframe = errors ? &frame : NULL;

          if(result == NULL)
          {
            // If we don't have a result yet, use this one.
            result = r;
          } else if(!is_subtype(r, result, pframe, opt)) {
            if(is_subtype(result, r, pframe, opt))
            {
              // Use the supertype function. Require the most specific
              // arguments and return the least specific result.
              // TODO: union the signatures, to handle arg names and
              // default arguments.
              if(errors)
                errorframe_discard(pframe);

              ast_free_unattached(result);
              result = r;
            } else {
              if(errors)
              {
                errorframe_t frame = NULL;
                ast_error_frame(&frame, from,
                  "a member of the union type has an incompatible method "
                  "signature");
                errorframe_append(&frame, pframe);
                errorframe_report(&frame, opt->check.errors);
              }

              ast_free_unattached(r);
              ok = false;
            }
          }
          break;
        }
      }
    }

    child = ast_sibling(child);
  }

  if(!ok)
  {
    ast_free_unattached(result);
    result = NULL;
  }

  return result;
}

static ast_t* lookup_isect(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name, bool errors)
{
  ast_t* child = ast_child(type);
  ast_t* result = NULL;
  bool ok = true;

  while(child != NULL)
  {
    ast_t* r = lookup_base(opt, from, child, child, name, false);

    if(r != NULL)
    {
      switch(ast_id(r))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED:
          // Ignore fields.
          break;

        default:
          if(result == NULL)
          {
            // If we don't have a result yet, use this one.
            result = r;
          } else if(!is_subtype(result, r, NULL, opt)) {
            if(is_subtype(r, result, NULL, opt))
            {
              // Use the subtype function. Require the least specific
              // arguments and return the most specific result.
              ast_free_unattached(result);
              result = r;
            }

            // TODO: isect the signatures, to handle arg names and
            // default arguments. This is done even when the functions have
            // no subtype relationship.
          }
          break;
      }
    }

    child = ast_sibling(child);
  }

  if(errors && (result == NULL))
    ast_error(opt->check.errors, from, "couldn't find '%s'", name);

  if(!ok)
  {
    ast_free_unattached(result);
    result = NULL;
  }

  return result;
}

static ast_t* lookup_base(pass_opt_t* opt, ast_t* from, ast_t* orig,
  ast_t* type, const char* name, bool errors)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return lookup_union(opt, from, type, name, errors);

    case TK_ISECTTYPE:
      return lookup_isect(opt, from, type, name, errors);

    case TK_TUPLETYPE:
      if(errors)
        ast_error(opt->check.errors, from, "can't lookup by name on a tuple");

      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(opt, from, orig, type, name, errors);

    case TK_ARROW:
      return lookup_base(opt, from, orig, ast_childidx(type, 1), name, errors);

    case TK_TYPEPARAMREF:
      return lookup_typeparam(opt, from, orig, type, name, errors);

    case TK_FUNTYPE:
      if(errors)
        ast_error(opt->check.errors, from,
          "can't lookup by name on a function type");

      return NULL;

    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      // Can only happen due to a local inference fail earlier
      return NULL;

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* lookup(pass_opt_t* opt, ast_t* from, ast_t* type, const char* name)
{
  return lookup_base(opt, from, type, type, name, true);
}

ast_t* lookup_try(pass_opt_t* opt, ast_t* from, ast_t* type, const char* name)
{
  return lookup_base(opt, from, type, type, name, false);
}
