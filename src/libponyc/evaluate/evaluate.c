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

    default:
      return 0;
  }
  return false;
}

typedef ast_t* (*method_ptr_t)(ast_t*, ast_t*, errorframe_t*);

typedef struct method_entry {
  const char* name;
  const method_ptr_t method;
} method_entry_t;

static method_entry_t methods[] = {
  { "add",    &evaluate_add },
  { "sub",    &evaluate_sub },
  { "op_and", &evaluate_and },
  { "op_or",  &evaluate_or },
  { NULL, NULL }
};

static method_ptr_t lookup_method(const char* operation) {
  for (int i = 0; methods[i].name != NULL; ++i) {
    if (!strcmp(operation, methods[i].name))
      return methods[i].method;
  }
  assert(0);
  return NULL;
}

ast_t* evaluate(ast_t* expression, errorframe_t* errors) {
  switch(ast_id(expression)) {
    case TK_TRUE:
    case TK_FALSE:
    case TK_INT:
      return expression;

    // FIXME: if we ensure everything typevalue is sugar with these
    // then can just always take child and never care
    // that this node even exists
    case TK_CONSTANT:
    {
      //TODO: not sure if you really want to rewrite here
      // or just evaluate -- speak with Sylvan
      // could pull the constant node out of here and rewrite outside
      // probably nicer
      ast_t *evaluated = evaluate(ast_child(expression), errors);
      ast_replace(&expression, evaluated);
      return expression;
    }

    // TODO: going to need some concept of state
    case TK_SEQ:
    {
      ast_t * evaluated;
      for(ast_t* p = ast_child(expression); p != NULL; p = ast_sibling(p))
      {
        evaluated = evaluate(p, errors);
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
        ast_error_frame(errors, expression,
          "No support for compile time expressions with positional arguments");

      AST_GET_CHILDREN(evaluate(lhs, errors), receiver, id);
      assert(ast_id(positional) == TK_POSITIONALARGS);
      return lookup_method(ast_name(id))(receiver, positional, errors);
      // will need to evaluate LHS
      assert(0);
    }

    default:
      if(errors != NULL)
        ast_error_frame(errors, expression, "Cannot evaluate compile time expression");
      break;
  }
  assert(0);
  return expression;
}

//TODO: evaluation of integer expression
ast_t* evaluate_integer_expression(ast_t* expression) {
  switch(ast_id(expression)) {
    case TK_INT:
      return expression;

    case TK_CALL: {
      // 1 is positional args
      // 2 is something
      // 3 is function application
      AST_GET_CHILDREN(expression, args, something, funapp);
      assert(0 && "Call");
    }

    case TK_DOT:
      return ast_child(expression);

    default:
      ast_error(expression, "Cannot handle %s", ast_get_print(expression));
      assert(0);
  }
  return expression;
}
