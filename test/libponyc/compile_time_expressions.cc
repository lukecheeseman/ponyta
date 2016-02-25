#include <gtest/gtest.h>
#include <platform.h>
#include <type/subtype.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))

class CompileTimeExpressionTest: public PassTest
{};

TEST_F(CompileTimeExpressionTest, EvaluateAndAssign)
{
  const char* src =
    "class C1\n"
    "  let x = #(1+2)";

  TEST_COMPILE(src);
}
