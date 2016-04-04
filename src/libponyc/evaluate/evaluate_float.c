#include "../evaluate/evaluate_float.h"
#include "../expr/literal.h"
#include "../type/subtype.h"
#include <assert.h>

static bool is_ast_float(ast_t* ast)
{
  return is_float(ast_type(ast)) || ast_id(ast) == TK_FLOAT;
}

ast_t* evaluate_add_float(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = evaluate(receiver);
  ast_t* rhs_arg = evaluate(ast_child(args));

  if(!is_ast_float(lhs_arg))
  {
    ast_error(rhs_arg, "%s is not a float expression", ast_get_print(lhs_arg));
    return NULL;
  }

  if(!is_ast_float(rhs_arg))
  {
    ast_error(rhs_arg, "%s is not a float expression", ast_get_print(rhs_arg));
    return NULL;
  }

  ast_t* result = ast_dup(lhs_arg);
  double lhs = ast_float(result);
  double rhs = ast_float(rhs_arg);

  ast_set_float(result, lhs+rhs); 
  return result; 
}


ast_t* evaluate_neg_float(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(evaluate(receiver));

  ast_set_float(result, -ast_float(result));

  return result;
}
