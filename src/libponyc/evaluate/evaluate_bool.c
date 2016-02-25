#include "../evaluate/evaluate_bool.h"

ast_t* evaluate_and_bool(ast_t* receiver, ast_t* args) {
  ast_t* lhs_arg = evaluate(receiver);
  if (ast_id(lhs_arg) == TK_TRUE)
    return evaluate(ast_child(args));

  return lhs_arg;
}

ast_t* evaluate_or_bool(ast_t* receiver, ast_t* args) {
  ast_t* lhs_arg = evaluate(receiver);
  if (ast_id(lhs_arg) == TK_FALSE)
    return evaluate(ast_child(args));

  return lhs_arg;
}
