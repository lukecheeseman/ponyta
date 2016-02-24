#include "../ast/ast.h"

ast_t* evaluate(ast_t* expression, errorframe_t* errors);
ast_t* evaluate_integer_expression(ast_t* expression);

bool equal(ast_t* expr_a, ast_t* expr_b);
