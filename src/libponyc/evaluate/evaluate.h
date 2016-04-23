#include "../ast/ast.h"
#include "../pass/pass.h"

ast_t* evaluate(ast_t* expression);
bool contains_valueparamref(ast_t* ast);
bool expr_constant(ast_t** astp);
bool ast_equal(ast_t* left, ast_t* right);
