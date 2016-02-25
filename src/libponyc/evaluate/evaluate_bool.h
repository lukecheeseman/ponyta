#include "../evaluate/evaluate.h"

ast_t* evaluate_and_bool(ast_t* receiver, ast_t* args);
ast_t* evaluate_or_bool(ast_t* receiver, ast_t* args);
ast_t* evaluate_not_bool(ast_t* receiver, ast_t* args);
