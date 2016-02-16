#include "sanitise.h"
#include "../ast/astbuild.h"
#include <assert.h>


// Collect the given type parameter
static void collect_type_param(ast_t* orig_param, ast_t* params, ast_t* args)
{
  assert(orig_param != NULL);
  assert(params != NULL);
  assert(args != NULL);

  // Get original type parameter info
  AST_GET_CHILDREN(orig_param, id, constraint, deflt);
  const char* name = ast_name(id);

  constraint = sanitise_type(constraint);
  assert(constraint != NULL);

  // New type parameter has the same constraint as the old one (sanitised)
  BUILD(new_param, orig_param,
    NODE(TK_TYPEPARAM,
      ID(name)
      TREE(constraint)
      NONE));

  ast_append(params, new_param);

  // New type arguments binds to old type parameter
  BUILD(new_arg, orig_param,
    NODE(TK_NOMINAL,
      NONE  // Package
      ID(name)
      NONE  // Type args
      NONE  // cap
      NONE)); // ephemeral

  ast_append(args, new_arg);

  // Since we have a type parameter the params and args node should not be
  // TK_NONE
  ast_setid(params, TK_TYPEPARAMS);
  ast_setid(args, TK_TYPEARGS);
}


void collect_type_params(ast_t* ast, ast_t** out_params, ast_t** out_args)
{
  assert(ast != NULL);
  assert(out_params != NULL);
  assert(out_args != NULL);

  // Create params and args as TK_NONE, we'll change them if we find any type
  // params
  ast_t* params = ast_from(ast, TK_NONE);
  ast_t* args = ast_from(ast, TK_NONE);

  ast_t* method = ast;

  // Find enclosing method
  while(ast_id(method) != TK_FUN && ast_id(method) != TK_NEW &&
    ast_id(method) != TK_BE)
  {
    method = ast_parent(method);
    assert(method != NULL);
  }

  // Find enclosing entity
  ast_t* entity = ast_parent(ast_parent(method));
  ast_t* entity_t_params = ast_childidx(entity, 1);

  // Collect type parameters defined on the entity
  for(ast_t* p = ast_child(entity_t_params); p != NULL; p = ast_sibling(p))
    collect_type_param(p, params, args);

  ast_t* method_t_params = ast_childidx(method, 2);

  // Collect type parameters defined on the method
  for(ast_t* p = ast_child(method_t_params); p != NULL; p = ast_sibling(p))
    collect_type_param(p, params, args);

  *out_params = params;
  *out_args = args;
}


// Sanitise the given type (sub)AST, which has already been copied 
static void sanitise(ast_t** astp)
{
  assert(astp != NULL);

  ast_t* type = *astp;
  assert(type != NULL);

  ast_clearflag(*astp, AST_FLAG_PASS_MASK);

  if(ast_id(type) == TK_TYPEPARAMREF)
  {
    // We have a type param reference, convert to a nominal
    ast_t* def = (ast_t*)ast_data(type);
    assert(def != NULL);

    const char* name = ast_name(ast_child(def));
    assert(name != NULL);

    REPLACE(astp,
      NODE(TK_NOMINAL,
        NONE      // Package name
        ID(name)
        NONE      // Type args
        NONE      // Capability
        NONE));   // Ephemeral

    return;
  }

  // Process all our children
  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
    sanitise(&p);
}


ast_t* sanitise_type(ast_t* type)
{
  assert(type != NULL);

  ast_t* new_type = ast_dup(type);
  sanitise(&new_type);
  return new_type;
}
