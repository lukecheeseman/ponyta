#include "../ast/astbuild.h"
#include "../evaluate/evaluate_int.h"
#include "../expr/literal.h"
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
  lexint->is_negative = !t.is_negative;
}

static int lexint_cmp_with_negative(lexint_t* lhs, lexint_t* rhs)
{
  if(lhs->is_negative && !rhs->is_negative)
    return -1;

  if(!lhs->is_negative && rhs->is_negative)
    return 1;

  return lexint_cmp(lhs, rhs);
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

typedef void (*lexint_method64_t)(lexint_t*, lexint_t*, uint64_t);

static ast_t* evaluate_div_mul_int(ast_t* receiver, ast_t* args, lexint_method64_t operation)
{
  // First make both arguments postive, perform the operation and then
  // negate the result if necessary
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_t rt = *rhs;
  lexint_t lt = *lhs;
  if(rt.is_negative)
    lexint_negate(&rt);

  if(lt.is_negative)
    lexint_negate(&lt);

  if(lexint_cmp64(&rt, 0xffffffffffffffff) == 1)
  {
    // FIXME: can only print 64 bit integers with ast_get_rpint
    // token method for printing should probably be changed then
    ast_error(rhs_arg, "Value %s is too large for multiplication", ast_get_print(rhs_arg));
    return NULL;
  }

  operation(lhs, &lt, rt.low);

  bool is_negative = lhs->is_negative ^ rhs->is_negative;
  if(is_negative)
    lexint_negate(lhs);
  lhs->is_negative = is_negative;

  return result;
}

ast_t* evaluate_mul_int(ast_t* receiver, ast_t* args)
{
  return evaluate_div_mul_int(receiver, args, &lexint_mul64);
}

ast_t* evaluate_div_int(ast_t* receiver, ast_t* args)
{
  return evaluate_div_mul_int(receiver, args, &lexint_div64);
}

ast_t* evaluate_neg_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(evaluate(receiver));

  lexint_t* result_int = ast_int(result);

  lexint_negate(result_int);
  return result;
}

typedef bool (*test_equality_t)(lexint_t*, lexint_t*);

static ast_t* evaluate_inequality(ast_t* receiver, ast_t* args, test_equality_t test)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  BUILD(result, lhs_arg, NODE(test(lhs, rhs) ? TK_TRUE : TK_FALSE));

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

static bool test_eq(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp_with_negative(lhs, rhs) == 0;
}

static bool test_ne(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp_with_negative(lhs, rhs) != 0;
}

static bool test_lt(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp_with_negative(lhs, rhs) < 0;
}

static bool test_le(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp_with_negative(lhs, rhs) <= 0;
}

static bool test_gt(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp_with_negative(lhs, rhs) > 0;
}

static bool test_ge(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp_with_negative(lhs, rhs) >= 0;
}

ast_t* evaluate_eq_int(ast_t* receiver, ast_t* args)
{
  return evaluate_inequality(receiver, args, &test_eq);
}

ast_t* evaluate_ne_int(ast_t* receiver, ast_t* args)
{
  return evaluate_inequality(receiver, args, &test_ne);
}

ast_t* evaluate_lt_int(ast_t* receiver, ast_t* args)
{
  return evaluate_inequality(receiver, args, &test_lt);
}

ast_t* evaluate_le_int(ast_t* receiver, ast_t* args)
{
  return evaluate_inequality(receiver, args, &test_le);
}

ast_t* evaluate_ge_int(ast_t* receiver, ast_t* args)
{
  return evaluate_inequality(receiver, args, &test_ge);
}

ast_t* evaluate_gt_int(ast_t* receiver, ast_t* args)
{
  return evaluate_inequality(receiver, args, &test_gt);
}

static ast_t* evaluate_min_max_int(ast_t* receiver, ast_t* args, test_equality_t test)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  if (test(lhs, rhs))
    return ast_dup(lhs_arg);

  return ast_dup(rhs_arg);
}

ast_t* evaluate_min_int(ast_t* receiver, ast_t* args)
{
  return evaluate_min_max_int(receiver, args, &test_lt);
}

ast_t* evaluate_max_int(ast_t* receiver, ast_t* args)
{
  return evaluate_min_max_int(receiver, args, &test_gt);
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

ast_t* evaluate_xor_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_xor(lhs, lhs, rhs);
  return result;
}

ast_t* evaluate_not_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(evaluate(receiver));

  lexint_t* result_int = ast_int(result);

  lexint_not(result_int, result_int);
  return result;
}

ast_t* evaluate_shl_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_shl(lhs, lhs, rhs->low);
  return result;
}

ast_t* evaluate_shr_int(ast_t* receiver, ast_t* args)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_shr(lhs, lhs, rhs->low);
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

ast_t* evaluate_hash_int(ast_t* receiver, ast_t* args)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(evaluate(receiver));
  lexint_t* result_int = ast_int(result);

  lexint_t lt;
  lexint_t rt;
  lexint_not(&lt, result_int);
  lexint_shl(&rt, result_int, 21);
  lexint_add(result_int, &lt, &rt);

  lexint_shr(&lt, result_int, 24);
  lexint_xor(result_int, result_int, &lt);

  lexint_shl(&lt, result_int, 3);
  lexint_add(&lt, result_int, &lt);
  lexint_shl(&rt, result_int, 8);
  lexint_add(result_int, &lt, &rt);

  lexint_shr(&lt, result_int, 14);
  lexint_xor(result_int, result_int, &lt);

  lexint_shl(&lt, result_int, 2);
  lexint_add(&lt, result_int, &lt);
  lexint_shl(&rt, result_int, 4);
  lexint_add(result_int, &lt, &rt);

  lexint_shr(&lt, result_int, 28);
  lexint_xor(result_int, result_int, &lt);

  lexint_shl(&lt, result_int, 31);
  lexint_add(result_int, result_int, &lt);

  // result will get duplicated in the cast method
  ast_t* final_result = cast_to_type(result, "U64");
  ast_free(result);
  return final_result;
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
