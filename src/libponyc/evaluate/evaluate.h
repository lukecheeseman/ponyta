#include "../ast/ast.h"
#include "../pass/pass.h"

ast_t* evaluate(pass_opt_t* opt, ast_t* expression, ast_t* scope);
bool evaluate_expressions(pass_opt_t* opt, ast_t** astp);
bool expr_constant(pass_opt_t* opt, ast_t** astp);
bool contains_valueparamref(ast_t* ast);
bool ast_equal(ast_t* left, ast_t* right);
