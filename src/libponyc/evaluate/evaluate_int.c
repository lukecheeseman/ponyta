#include "../evaluate/evaluate_int.h"
#include "../type/subtype.h"
#include <assert.h>

// FIXME: These should not overwrite the existing values
// will probably need to create a new token and a new ast node
// perhaps we could get away with not knowing the type of a literal before we
// evaluate it....still not great though surely
ast_t* evaluate_create(ast_t* receiver, ast_t* args)
{
  if(receiver){}
  return evaluate(ast_child(args));
}

ast_t* evaluate_add(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg = evaluate(receiver);
  ast_t* rhs_arg = evaluate(ast_child(args));

  assert(is_integer(ast_type(lhs_arg)) && is_integer(ast_type(rhs_arg)));

  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_add(lhs, lhs, rhs);
  return receiver;
}

ast_t* evaluate_sub(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg = evaluate(receiver);
  ast_t* rhs_arg = evaluate(ast_child(args));
  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_sub(lhs, lhs, rhs);
  return receiver;
}

ast_t* evaluate_create_u32(ast_t* receiver, ast_t* args) {
  return evaluate_create(receiver, args);
}

ast_t* evaluate_add_u32(ast_t* receiver, ast_t* args) {
  return evaluate_add(receiver, args);
}

ast_t* evaluate_sub_u32(ast_t* receiver, ast_t* args) {
  return evaluate_sub(receiver, args);
}

ast_t* evaluate_add_usize(ast_t* receiver, ast_t* args) {
  return evaluate_add(receiver, args);
}

ast_t* evaluate_sub_usize(ast_t* receiver, ast_t* args) {
  return evaluate_sub(receiver, args);
}
