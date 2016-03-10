#include "../ast/ast.h"

ast_t* evaluate(ast_t* expression);
bool equal(ast_t* expr_a, ast_t* expr_b);
bool contains_valueparamref(ast_t* ast);
bool expr_constant(ast_t* ast);
