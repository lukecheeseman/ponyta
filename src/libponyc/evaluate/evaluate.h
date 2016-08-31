#ifndef EVALUATE_H
#define EVALUATE_H

#include "../ast/ast.h"
#include "../pass/pass.h"

bool evaluate_expressions(pass_opt_t* opt, ast_t** astp);
bool evaluate_expression(pass_opt_t* opt, ast_t** astp);
bool expr_constant(pass_opt_t* opt, ast_t** astp);
bool contains_valueparamref(ast_t* ast);
bool ast_equal(ast_t* left, ast_t* right);

void methodtab_init();
void methodtab_done();

void eval_cache_init();
void eval_cache_done();

#endif
