#include "evaluate_vector.h"
#include <assert.h>

ast_t* evaluate_apply_vector(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  (void) opt;
  lexint_t* index = ast_int(ast_child(args));
  ast_t* elements = ast_childidx(receiver, 1);
  return ast_childidx(elements, index->low);
}
