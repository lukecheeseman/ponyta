#include "../ast/astbuild.h"
#include "../evaluate/evaluate_int.h"
#include "../expr/literal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include <assert.h>

static bool is_ast_integer(ast_t* ast)
{
  return ast_id(ast) == TK_INT;
}

ast_t* evaluate_create_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  (void) receiver;
  (void) opt;
  return ast_child(args);
}

static bool check_operands(ast_t* lhs_arg, ast_t* rhs_arg, pass_opt_t* opt)
{
  if(lhs_arg == NULL || rhs_arg == NULL)
    return false;

  if(!is_ast_integer(lhs_arg))
  {
    ast_error(opt->check.errors, rhs_arg,
      "%s is not a compile-time integer expression", ast_get_print(lhs_arg));
    return false;
  }

  if(!is_ast_integer(rhs_arg))
  {
    ast_error(opt->check.errors, rhs_arg,
      "%s is not a compile-time integer expression", ast_get_print(rhs_arg));
    return false;
  }

  return true; 
}

typedef void (*binary_int_operation_t)(lexint_t*, lexint_t*, lexint_t*);

static ast_t* evaluate_binary_int_operation(ast_t* receiver, ast_t* args,
  binary_int_operation_t operation, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);
  if(!check_operands(lhs_arg, rhs_arg, opt))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  operation(lhs, lhs, rhs);
  return result;
}

ast_t* evaluate_add_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_int_operation(receiver, args, &lexint_add, opt);
}

ast_t* evaluate_sub_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_int_operation(receiver, args, &lexint_sub, opt);
}

ast_t* evaluate_and_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_int_operation(receiver, args, &lexint_and, opt);
}

ast_t* evaluate_or_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_int_operation(receiver, args, &lexint_or, opt);
}

ast_t* evaluate_xor_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_int_operation(receiver, args, &lexint_xor, opt);
}

typedef void (*binary_int64_operation)(lexint_t*, lexint_t*, uint64_t);

static ast_t* evaluate_binary_int64_operation(ast_t* receiver, ast_t* args,
  binary_int64_operation operation, pass_opt_t* opt)
{
  // First make both arguments postive, perform the operation and then
  // negate the result if necessary
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);
  if(!check_operands(lhs_arg, rhs_arg, opt))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_t rt = *rhs;
  lexint_t lt = *lhs;
  if(rt.is_negative)
    lexint_negate(&rt, &rt);

  if(lt.is_negative)
    lexint_negate(&lt, &lt);

  if(lexint_cmp64(&rt, 0xffffffffffffffff) == 1)
  {
    // FIXME: can only print 64 bit integers with ast_get_rpint
    // token method for printing should probably be changed then
    ast_error(opt->check.errors, rhs_arg, "Value %s is too large for multiplication", ast_get_print(rhs_arg));
    return NULL;
  }

  operation(lhs, &lt, rt.low);

  bool is_negative = lhs->is_negative ^ rhs->is_negative;
  if(is_negative)
    lexint_negate(lhs, lhs);
  lhs->is_negative = is_negative;

  return result;
}

ast_t* evaluate_mul_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_int64_operation(receiver, args, &lexint_mul64, opt);
}

ast_t* evaluate_div_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_int64_operation(receiver, args, &lexint_div64, opt);
}

ast_t* evaluate_neg_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  (void) opt;
  if(!is_ast_integer(receiver))
    return NULL;

  ast_t* result = ast_dup(receiver);

  lexint_t* result_int = ast_int(result);

  lexint_negate(result_int, result_int);
  return result;
}

typedef bool (*test_equality_t)(lexint_t*, lexint_t*);

static ast_t* evaluate_inequality_int(ast_t* receiver, ast_t* args,
  test_equality_t test, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);
  if(!check_operands(lhs_arg, rhs_arg, opt))
    return NULL;

  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  BUILD(result, lhs_arg, NODE(test(lhs, rhs) ? TK_TRUE : TK_FALSE));

  ast_t* bool_type = type_builtin(opt, ast_type(lhs_arg), "Bool");

  ast_settype(result, bool_type);
  return result;
}

static bool test_eq(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp(lhs, rhs) == 0;
}

static bool test_ne(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp(lhs, rhs) != 0;
}

static bool test_lt(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp(lhs, rhs) < 0;
}

static bool test_le(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp(lhs, rhs) <= 0;
}

static bool test_gt(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp(lhs, rhs) > 0;
}

static bool test_ge(lexint_t* lhs, lexint_t* rhs)
{
  return lexint_cmp(lhs, rhs) >= 0;
}

ast_t* evaluate_eq_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_int(receiver, args, &test_eq, opt);
}

ast_t* evaluate_ne_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_int(receiver, args, &test_ne, opt);
}

ast_t* evaluate_lt_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_int(receiver, args, &test_lt, opt);
}

ast_t* evaluate_le_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_int(receiver, args, &test_le, opt);
}

ast_t* evaluate_ge_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_int(receiver, args, &test_ge, opt);
}

ast_t* evaluate_gt_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_int(receiver, args, &test_gt, opt);
}

static ast_t* evaluate_min_max_int(ast_t* receiver, ast_t* args,
  test_equality_t test, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);
  if(!check_operands(lhs_arg, rhs_arg, opt))
    return NULL;

  lexint_t* lhs = ast_int(lhs_arg);
  lexint_t* rhs = ast_int(rhs_arg);

  if (test(lhs, rhs))
    return ast_dup(lhs_arg);

  return ast_dup(rhs_arg);
}

ast_t* evaluate_min_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_min_max_int(receiver, args, &test_lt, opt);
}

ast_t* evaluate_max_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_min_max_int(receiver, args, &test_gt, opt);
}

ast_t* evaluate_not_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  (void) opt;
  if(!is_ast_integer(receiver))
    return NULL;

  ast_t* result = ast_dup(receiver);

  lexint_t* result_int = ast_int(result);

  lexint_not(result_int, result_int);
  return result;
}

ast_t* evaluate_shl_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);
  if(!check_operands(lhs_arg, rhs_arg, opt))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_shl(lhs, lhs, rhs->low);
  return result;
}

ast_t* evaluate_shr_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);
  if(!check_operands(lhs_arg, rhs_arg, opt))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  lexint_t* lhs = ast_int(result);
  lexint_t* rhs = ast_int(rhs_arg);

  lexint_shr(lhs, lhs, rhs->low);
  return result;
}

// casting methods
static void cast_to_type(ast_t* receiver, const char* type, pass_opt_t* opt) {
  ast_t* new_type = type_builtin(opt, ast_type(receiver), type);
  assert(new_type);

  ast_settype(receiver, new_type);
}

ast_t* evaluate_hash_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  if(!is_ast_integer(receiver))
    return NULL;

  ast_t* result = ast_dup(receiver);
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
  cast_to_type(result, "U64", opt);
  return result;
}

ast_t* evaluate_i8_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "I8", opt);
  return result;
}

ast_t* evaluate_i16_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "I16", opt);
  return result;
}

ast_t* evaluate_i32_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "I32", opt);
  return result;
}

ast_t* evaluate_i64_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "I64", opt);
  return result;
}

ast_t* evaluate_i128_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "I128", opt);
  return result;
}

ast_t* evaluate_ilong_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "ILong", opt);
  return result;
}

ast_t* evaluate_isize_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "ISize", opt);
  return result;
}

ast_t* evaluate_u8_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "U8", opt);
  return result;
}

ast_t* evaluate_u16_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "U16", opt);
  return result;
}

ast_t* evaluate_u32_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "U32", opt);
  return result;
}

ast_t* evaluate_u64_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "U64", opt);
  return result;
}

ast_t* evaluate_u128_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "U128", opt);
  return result;
}

ast_t* evaluate_ulong_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "ULong", opt);
  return result;
}

ast_t* evaluate_usize_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  ast_t* result = ast_dup(receiver);
  cast_to_type(result, "USize", opt);
  return result;
}

static ast_t* cast_int_to_float(ast_t* receiver, const char* type,
  pass_opt_t* opt)
{
  //FIXME
  lexint_t* evaluated_int = ast_int(receiver);

  double result_double;
  if(evaluated_int->is_negative && is_signed(receiver))
  {
    lexint_t t = *evaluated_int;
    lexint_negate(&t, &t);
    result_double = -lexint_double(&t);
  } else {
    result_double = lexint_double(evaluated_int);
  }

  ast_t* result = ast_from(receiver, TK_FLOAT);
  ast_set_float(result, result_double);
  cast_to_type(result, type, opt);
  return result;
}

ast_t* evaluate_f32_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  return cast_int_to_float(receiver, "F32", opt);
}

ast_t* evaluate_f64_int(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  return cast_int_to_float(receiver, "F64", opt);
}
