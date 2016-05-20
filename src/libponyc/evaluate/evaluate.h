#ifndef EVALUATE_H
#define EVALUATE_H

#include "../ast/ast.h"
#include "../pass/pass.h"
#include "../../libponyrt/ds/hash.h"

ast_t* evaluate(pass_opt_t* opt, ast_t* expression, ast_t* scope);
bool evaluate_expressions(pass_opt_t* opt, ast_t** astp);
bool expr_constant(pass_opt_t* opt, ast_t** astp);
bool contains_valueparamref(ast_t* ast);
bool ast_equal(ast_t* left, ast_t* right);

typedef ast_t* (*method_ptr_t)(ast_t*, ast_t*, pass_opt_t* opt);

typedef struct method_entry_t {
  const char* type;
  const char* name;
  const method_ptr_t method;
} method_entry_t;

DECLARE_HASHMAP(method_table, method_table_t, method_entry_t);

#endif
