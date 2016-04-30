#include "../evaluate/evaluate_bool.h"

ast_t* evaluate_and_bool(ast_t* receiver, ast_t* args) {
  return (ast_id(receiver) == TK_TRUE) ? ast_child(args): receiver;
}

ast_t* evaluate_or_bool(ast_t* receiver, ast_t* args) {
  return (ast_id(receiver) == TK_FALSE) ? ast_child(args): receiver;
}
