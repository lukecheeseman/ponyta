#include "../evaluate/evaluate_bool.h"
#include "../ast/astbuild.h"

ast_t* evaluate_and_bool(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  (void) opt;
  return (ast_id(receiver) == TK_TRUE) ? ast_child(args): receiver;
}

ast_t* evaluate_or_bool(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  (void) opt;
  return (ast_id(receiver) == TK_FALSE) ? ast_child(args): receiver;
}

ast_t* evaluate_not_bool(ast_t* receiver, ast_t* args, pass_opt_t* opt)

{
  (void) opt;
  (void) args;
  BUILD(result, receiver, NODE(ast_id(receiver) == TK_FALSE ? TK_TRUE : TK_FALSE));
  return result;
}
