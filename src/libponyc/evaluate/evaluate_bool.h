#include "../evaluate/evaluate.h"

ast_t* evaluate_and_bool(ast_t* receiver, ast_t* args, errorframe_t* errors);
ast_t* evaluate_or_bool(ast_t* receiver, ast_t* args, errorframe_t* errors);
ast_t* evaluate_not_bool(ast_t* receiver, ast_t* args, errorframe_t* errors);
