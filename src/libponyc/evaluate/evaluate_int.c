#include "../ast/astbuild.h"
#include "../evaluate/evaluate_int.h"
#include "../type/subtype.h"
#include <assert.h>

// FIXME: These should not overwrite the existing values
// will probably need to create a new token and a new ast node

static bool is_ast_integer(ast_t* ast)
{
  return is_integer(ast_type(ast)) || ast_id(ast) == TK_INT;
}

ast_t* evaluate_create_int(ast_t* receiver, ast_t* args)
{
  assert(receiver);
  return evaluate(ast_child(args));
}

static void lexint_negate(lexint_t* lexint)
{
  lexint_t t = *lexint;
  lexint_zero(lexint);
  lexint_sub(lexint, lexint, &t);
  lexint->is_negative = !lexint->is_negative;
}

static bool get_operands(ast_t* receiver, ast_t* args, ast_t** lhs, ast_t** rhs)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = evaluate(receiver);
  ast_t* rhs_arg = evaluate(ast_child(args));

  if(!is_ast_integer(lhs_arg))
  {
    ast_error(rhs_arg, "%s is not an integer expression", ast_get_print(lhs_arg));
    return false;
  }

  if(!is_ast_integer(rhs_arg))
  {
    ast_error(rhs_arg, "%s is not an integer expression", ast_get_print(rhs_arg));
    return false;
  }

  *lhs = lhs_arg;
  *rhs = rhs_arg;
  return true; 
}

ast_t* evaluate_add_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  if(lhs->is_negative && !rhs->is_negative)
  {
    lexint_t t = *lhs;
    lexint_negate(&t);
    lhs->is_negative = lexint_cmp(&t, rhs) > 0;
  }
  else if(!lhs->is_negative && rhs->is_negative)
  {
    lexint_t t = *rhs;
    lexint_negate(&t);
    lhs->is_negative = lexint_cmp(&t, lhs) > 0;
  }

  lexint_add(lhs, lhs, rhs);
  return result;
}

ast_t* evaluate_sub_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);


  if(lhs->is_negative && rhs->is_negative)
  {
    lhs->is_negative = lexint_cmp(lhs, rhs) > 0;
  }
  else if(!lhs->is_negative && !rhs->is_negative)
  {
    lhs->is_negative = lexint_cmp(lhs, rhs) < 0;
  }

  lexint_sub(lhs, lhs, rhs);
  return result;
}

ast_t* evaluate_mul_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  if(lexint_cmp64(rhs, 0xffffffffffffffff) == 1)
  {
    // FIXME: can only print 64 bit integers with ast_get_rpint
    // token method for printing should probably be changed then
    ast_error(rhs_arg, "Value %s is too large for multiplication", ast_get_print(rhs_arg));
    return NULL;
  }

  lexint_mul64(lhs, lhs, rhs->low);
  return result;
}

ast_t* evaluate_div_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  if(lexint_cmp64(rhs, 0xffffffffffffffff) == 1)
  {
    // FIXME: can only print 64 bit integers with ast_get_rpint
    // token method for printing should probably be changed then
    ast_error(rhs_arg, "Value %s is too large for multiplication", ast_get_print(rhs_arg));
    return NULL;
  }

  lexint_div64(lhs, lhs, rhs->low);
  return result;
}

ast_t* evaluate_neg_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = evaluate(receiver);

  lexint_t* result_int = ast_int(result);

  lexint_negate(result_int);
  return result;
}

ast_t* evaluate_eq_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  bool eq = lexint_cmp(lhs, rhs) == 0;
  BUILD(result, lhs_arg, NODE(eq ? TK_TRUE : TK_FALSE));

  ast_t* new_type_data = ast_get_case(receiver, "Bool", NULL);
  assert(new_type_data);

  // FIXME: probably better to build the type
  ast_t* new_type = ast_dup(ast_type(lhs_arg));
  ast_t* id = ast_childidx(new_type, 1);
  ast_replace(&id, ast_from_string(id,"Bool"));
  ast_setdata(new_type, new_type_data);

  ast_settype(result, new_type);
  return result;
}

ast_t* evaluate_and_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_and(lhs, lhs, rhs);
  return result;
}

ast_t* evaluate_or_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_or(lhs, lhs, rhs);
  return result;
}

// casting methods
static ast_t* cast_to_type(ast_t* receiver, const char* type) {
  ast_t* result = ast_dup(evaluate(receiver));

  ast_t* new_type_data = ast_get_case(receiver, type, NULL);
  assert(new_type_data);

  ast_t* new_type = ast_dup(ast_type(result));
  ast_t* id = ast_childidx(new_type, 1);
  ast_replace(&id, ast_from_string(id, type));
  ast_setdata(new_type, new_type_data);

  ast_settype(result, new_type);
  return result;
}

ast_t* evaluate_i8_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "I8");
}

ast_t* evaluate_i16_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "I16");
}

ast_t* evaluate_i32_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "I32");
}

ast_t* evaluate_i64_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "I64");
}

ast_t* evaluate_i128_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "I128");
}

ast_t* evaluate_ilong_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "ILong");
}

ast_t* evaluate_isize_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "ISize");
}

ast_t* evaluate_u8_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "U8");
}

ast_t* evaluate_u16_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "U16");
}

ast_t* evaluate_u32_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "U32");
}

ast_t* evaluate_u64_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "U64");
}

ast_t* evaluate_u128_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "U128");
}

ast_t* evaluate_ulong_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "ULong");
}

ast_t* evaluate_usize_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  return cast_to_type(receiver, "USize");
}
