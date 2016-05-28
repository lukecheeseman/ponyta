#include "../evaluate/evaluate_string.h"
#include <assert.h>
#include <string.h>

ast_t* evaluate_add_string(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  (void) opt;
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);

  const char* lhs_str = ast_name(lhs_arg);
  const char* rhs_str = ast_name(rhs_arg);

  size_t lhs_len = ast_name_len(lhs_arg);
  size_t rhs_len = ast_name_len(rhs_arg);

  char concat[lhs_len + rhs_len + 2];
  strncpy(concat, lhs_str, lhs_len + 1);
  strncat(concat, rhs_str, rhs_len + 1);

  ast_t* result = ast_dup(lhs_arg);
  ast_set_name(result, stringtab(concat));
  return result;
}
