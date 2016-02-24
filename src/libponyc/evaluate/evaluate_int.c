#include "../evaluate/evaluate_int.h"

ast_t* evaluate_add(ast_t* receiver, ast_t* args, errorframe_t* errors)
{
  ast_t* lhs_arg = evaluate(receiver, errors);
  ast_t* rhs_arg = evaluate(ast_child(args), errors);
  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_add(lhs, lhs, rhs);
  return receiver;
}

ast_t* evaluate_sub(ast_t* receiver, ast_t* args, errorframe_t* errors)
{
  ast_t* lhs_arg = evaluate(receiver, errors);
  ast_t* rhs_arg = evaluate(ast_child(args), errors);
  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_sub(lhs, lhs, rhs);
  return receiver;
}
