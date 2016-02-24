#include "../evaluate/evaluate.h"

ast_t* evaluate_and(ast_t* receiver, ast_t* args, errorframe_t* errors);
ast_t* evaluate_or(ast_t* receiver, ast_t* args, errorframe_t* errors);
