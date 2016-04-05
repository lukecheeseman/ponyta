#include "../evaluate/evaluate.h"

ast_t* evaluate_add_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_sub_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_mul_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_div_float(ast_t* receiver, ast_t* args);

ast_t* evaluate_neg_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_eq_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_ne_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_lt_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_le_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_ge_float(ast_t* receiver, ast_t* args);
ast_t* evaluate_gt_float(ast_t* receiver, ast_t* args);
