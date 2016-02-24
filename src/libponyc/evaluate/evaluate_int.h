#include "../evaluate/evaluate.h"

ast_t* evaluate_add(ast_t* receiver, ast_t* args, errorframe_t* errors);
ast_t* evaluate_sub(ast_t* receiver, ast_t* args, errorframe_t* errors);
