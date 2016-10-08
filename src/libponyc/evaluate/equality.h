#ifndef EQUALITY_H
#define EQUALITY_H

#include "../ast/ast.h"

// Build up a list of expressions which need to be equivalent
typedef struct equality_list_t
{
  ast_t* lhs;
  ast_t* rhs;
  struct equality_list_t* next;
} equality_list_t;

typedef struct equality_entry_t
{
  const char* type;
  const char* member;
  equality_list_t* head;
  equality_list_t* current;
} equality_entry_t;

DECLARE_HASHMAP(equality_tab, equality_tab_t, equality_entry_t);

equality_entry_t* search_equality(equality_tab_t* table, const char* type_name,
                                  const char* member_name);

void mark_check_equality(equality_tab_t* table, const char* type_name,
                         const char* member_name, ast_t* lhs, ast_t* rhs);

equality_tab_t* equality_tab_new();

bool ast_equal(ast_t* left, ast_t* right);

#endif
