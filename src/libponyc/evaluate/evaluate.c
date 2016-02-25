#include "evaluate.h"
#include <assert.h>
#include "string.h"
#include "../evaluate/evaluate_int.h"
#include "../evaluate/evaluate_bool.h"

// TODO: evaluation will have to take place likely before/in reify
// or do we say:
// append(v1: Vector[T, # n], v2: Vector[T, # m]) : Vector[T, #(n + m)]
// reifies to
// append(v1: Vector[T, # 4], v2: Vector[T, # 2]) : Vector[T, #(4 + 2)]
// and the when we assign it to a Vector[T, 6] then we check this way
// may be easier as the evaluation will be available and can be used once
// typesafe
// as another point, how do we right the above signature as we don't know
// m
// in C++ we have to template this to know what m will be
// but i guess here we know the type?

// TODO: research interpreters

bool equal(ast_t* expr_a, ast_t* expr_b) {
  switch(ast_id(expr_a)) {
    case TK_INT: {
      lexint_t *sub_value = ast_int(expr_a);
      lexint_t *super_value = ast_int(expr_b);
      return !lexint_cmp(sub_value, super_value);
    }

    case TK_TRUE:
    case TK_FALSE:
      return ast_id(expr_a) == ast_id(expr_b);

    case TK_VALUEFORMALPARAMREF:
      // FIXME: dicuss this, at this point we know they types are
      // okay but we won't know if the equality works until we
      // reify the type
      // should we even handle this?
      return true;

    default:
      assert(0);
  }
  return false;
}

typedef ast_t* (*method_ptr_t)(ast_t*, ast_t*);

typedef struct method_entry {
  const char* type;
  const char* name;
  const method_ptr_t method;
} method_entry_t;

// This table will have to be updated to be in the relevant classes
static method_entry_t method_table[] = {
  // u32 operations
  { "U32", "add",    &evaluate_add_u32 },
  { "U32", "sub",    &evaluate_sub_u32 },

  // boolean operations
  { "Bool", "op_and", &evaluate_and_bool },
  { "Bool", "op_or",  &evaluate_or_bool },

  // no entry in method table
  { NULL, NULL, NULL }
};

static method_ptr_t lookup_method(ast_t* type, const char* operation) {
  // At this point expressions should have been type and so any missing method_table
  // or method calls on bad types should have been caught
  assert(ast_id(type) == TK_NOMINAL);
  const char* type_name = ast_name(ast_childidx(type, 1));

  for (int i = 0; method_table[i].name != NULL; ++i) {
    if (!strcmp(type_name, method_table[i].type) && !strcmp(operation, method_table[i].name))
      return method_table[i].method;
  }
  assert(0);
  return NULL;
}

ast_t* evaluate(ast_t* expression) {
  switch(ast_id(expression)) {
    case TK_VALUEFORMALPARAMREF:
    case TK_NONE:
    case TK_TRUE:
    case TK_FALSE:
    case TK_INT:
      return expression;

    // TODO: going to need some concept of state
    case TK_SEQ:
    {
      ast_t * evaluated;
      for(ast_t* p = ast_child(expression); p != NULL; p = ast_sibling(p))
      {
        evaluated = evaluate(p);
      }
      assert(evaluated != NULL);
      return evaluated;
    }

    case TK_FUNREF:
      return expression;

    case TK_CALL:
    {
      AST_GET_CHILDREN(expression, positional, namedargs, lhs);
      if(ast_id(namedargs) != TK_NONE)
        ast_error(expression,
          "No support for compile time expressions with positional arguments");

      AST_GET_CHILDREN(evaluate(lhs), receiver, id);
      assert(ast_id(positional) == TK_POSITIONALARGS);
      return lookup_method(ast_type(receiver), ast_name(id))(receiver, positional);
      // will need to evaluate LHS
      assert(0);
    }

    default:
      ast_error(expression, "Cannot evaluate compile time expression");
      break;
  }
  assert(0);
  return expression;
}
