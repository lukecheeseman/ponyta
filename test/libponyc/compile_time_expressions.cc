#include <gtest/gtest.h>
#include <platform.h>
#include <type/subtype.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))

class CompileTimeExpressionTest: public PassTest
{};

TEST_F(CompileTimeExpressionTest, DISABLED_EvaluateAndAssign)
{
  const char* src =
    "class C1\n"
    "  let x: U32 = #(1+2)";

  TEST_COMPILE(src);
}

// FIXME: this test though
TEST_F(CompileTimeExpressionTest, DISABLED_EvaluateValueDependentTypeAndAssign)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  let c: C1[5] = C1[#(2+3)]";

  TEST_COMPILE(src);
}
