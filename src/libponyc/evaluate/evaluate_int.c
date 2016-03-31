#include "../evaluate/evaluate_int.h"
#include "../type/subtype.h"
#include <assert.h>

// FIXME: These should not overwrite the existing values
// will probably need to create a new token and a new ast node

// perhaps we could get away with not knowing the type of a literal before we
// evaluate it....still not great though surely
static bool is_ast_integer(ast_t* ast)
{
  return is_integer(ast_type(ast)) || ast_id(ast) == TK_INT;
}

ast_t* evaluate_create_int(ast_t* receiver, ast_t* args)
{
  assert(receiver);
  return evaluate(ast_child(args));
}

ast_t* evaluate_add_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg = evaluate(receiver);
  ast_t* rhs_arg = evaluate(ast_child(args));

  if(!is_ast_integer(lhs_arg))
  {
    ast_error(rhs_arg, "%s is not an integer expression", ast_get_print(lhs_arg));
    return NULL;
  }

  if(!is_ast_integer(rhs_arg))
  {
    ast_error(rhs_arg, "%s is not an integer expression", ast_get_print(rhs_arg));
    return NULL;
  }

  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_add(lhs, lhs, rhs);
  return receiver;
}

ast_t* evaluate_sub_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg = evaluate(receiver);
  ast_t* rhs_arg = evaluate(ast_child(args));
  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_sub(lhs, lhs, rhs);
  return receiver;
}
