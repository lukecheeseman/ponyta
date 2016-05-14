#include "../evaluate/evaluate.h"
#include "../pass/pass.h"

ast_t* evaluate_and_bool(ast_t* receiver, ast_t* args, pass_opt_t* opt);
ast_t* evaluate_or_bool(ast_t* receiver, ast_t* args, pass_opt_t* opt);
ast_t* evaluate_not_bool(ast_t* receiver, ast_t* args, pass_opt_t* opt);
