#include "../ast/astbuild.h"
#include "../evaluate/evaluate_float.h"
#include "../expr/literal.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include <assert.h>

static bool is_ast_float(ast_t* ast)
{
  return ast_id(ast) == TK_FLOAT;
}

typedef double (*binary_float_operation_t)(double, double);

static bool get_operands(ast_t* receiver, ast_t* args, ast_t** lhs, ast_t** rhs,
  pass_opt_t* opt)
{
  assert(ast_id(args) == TK_POSITIONALARGS);
  ast_t* lhs_arg = receiver;
  ast_t* rhs_arg = ast_child(args);

  if(!is_ast_float(lhs_arg))
  {
    ast_error(opt->check.errors, rhs_arg,
      "%s is not a float expression", ast_get_print(lhs_arg));
    return false;
  }

  if(!is_ast_float(rhs_arg))
  {
    ast_error(opt->check.errors, rhs_arg,
      "%s is not a float expression", ast_get_print(rhs_arg));
    return false;
  }

  *lhs = lhs_arg;
  *rhs = rhs_arg;
  return true;
}

static ast_t* evaluate_binary_float_operation(ast_t* receiver, ast_t* args,
  binary_float_operation_t operation, pass_opt_t* opt)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg, opt))
    return NULL;

  ast_t* result = ast_dup(lhs_arg);
  double lhs = ast_float(result);
  double rhs = ast_float(rhs_arg);

  ast_set_float(result, operation(lhs, rhs));
  return result;
}

static double add_float(double x, double y) { return x + y; }
static double sub_float(double x, double y) { return x - y; }
static double mul_float(double x, double y) { return x * y; }
static double div_float(double x, double y) { return x / y; }

ast_t* evaluate_add_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_float_operation(receiver, args, &add_float, opt);
}

ast_t* evaluate_sub_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_float_operation(receiver, args, &sub_float, opt);
}

ast_t* evaluate_mul_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_float_operation(receiver, args, &mul_float, opt);
}

ast_t* evaluate_div_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_binary_float_operation(receiver, args, &div_float, opt);
}

ast_t* evaluate_neg_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  assert(ast_id(args) == TK_NONE);
  (void) opt;
  ast_t* result = ast_dup(receiver);

  ast_set_float(result, -ast_float(result));

  return result;
}

typedef bool (*test_equality_double_t)(double, double);

static ast_t* evaluate_inequality_float(ast_t* receiver, ast_t* args,
  test_equality_double_t test, pass_opt_t* opt)
{
  ast_t* lhs_arg;
  ast_t* rhs_arg;
  if(!get_operands(receiver, args, &lhs_arg, &rhs_arg,opt))
    return NULL;

  double lhs = ast_float(lhs_arg);
  double rhs = ast_float(rhs_arg);

  BUILD(result, lhs_arg, NODE(test(lhs, rhs) ? TK_TRUE : TK_FALSE));

  ast_t* bool_type = type_builtin(opt, ast_type(lhs_arg), "Bool");

  ast_settype(result, bool_type);
  return result;
}

static bool test_eq_float(double x, double y) { return x == y; }
static bool test_ne_float(double x, double y) { return x != y; }
static bool test_lt_float(double x, double y) { return x < y; }
static bool test_le_float(double x, double y) { return x <= y; }
static bool test_gt_float(double x, double y) { return x > y; }
static bool test_ge_float(double x, double y) { return x >= y; }

ast_t* evaluate_eq_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_float(receiver, args, &test_eq_float, opt);
}

ast_t* evaluate_ne_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_float(receiver, args, &test_ne_float, opt);
}

ast_t* evaluate_lt_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_float(receiver, args, &test_lt_float, opt);
}

ast_t* evaluate_le_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_float(receiver, args, &test_le_float, opt);
}

ast_t* evaluate_gt_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_float(receiver, args, &test_gt_float, opt);
}

ast_t* evaluate_ge_float(ast_t* receiver, ast_t* args, pass_opt_t* opt)
{
  return evaluate_inequality_float(receiver, args, &test_ge_float, opt);
}
