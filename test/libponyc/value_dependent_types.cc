#include <gtest/gtest.h>
#include <platform.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))

class VDTTest: public PassTest
{};

TEST_F(VDTTest, TraitWithParameterValue)
{
  const char* src =
    "trait T1[n: U32]";

  TEST_COMPILE(src);
}

TEST_F(VDTTest, ParameterValueUsedInMethod)
{
  const char* src =
    "trait T1[n: U32]\n"
    "  fun get_n(): U32 =>\n"
    "    n";

  TEST_COMPILE(src);
}

TEST_F(VDTTest, ParameterValueUsedInMethodWrongType)
{
  const char* src =
    "trait T1[n: U32]\n"
    "  fun get_n(): U64 =>\n"
    "    n";

  TEST_ERROR(src);
}

TEST_F(VDTTest, TestBoolOutsideU32Constraint)
{
  const char* src =
    "trait T1[n: U32]\n"
    "class C1 is T1[true]";

  TEST_ERROR(src);
}

TEST_F(VDTTest, ClassWithValueDependentTrait)
{
  const char* src =
    "trait T1[n: U32]\n"
    "class C1 is T1[4]";

  TEST_COMPILE(src);
}

TEST_F(VDTTest, ClassWithParameterValue)
{
  const char* src =
    "class C1[n: U32]";

  TEST_COMPILE(src);
}

TEST_F(VDTTest, ClassWithMatchingArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  let c: C1[2] = C1[2]";

  TEST_COMPILE(src);
}


TEST_F(VDTTest, ClassWithMismatchValueArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  let c: C1[47] = C1[2]";

  TEST_ERROR(src);
}

TEST_F(VDTTest, ClassWithDeclareTypeMismatchValueArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  let c: C1[true] = C1[2]";

  TEST_ERROR(src);
}

TEST_F(VDTTest, ClassWithArgumentTypeMismatchValueArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  let c: C1[2] = C1[true]";

  TEST_ERROR(src);
}

TEST_F(VDTTest, FunctionCallWithFieldArgumentTypeMismatchValueArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  let c: C1[let n = 2] = C1[let n = 2]"
    "  fun get(f: C1[let n = 1]) =>\n"
    "    true\n"
    "  new create() =>\n"
    "    get(c1)";

  TEST_ERROR(src);
}

TEST_F(VDTTest, FunctionCallWithFieldTypeValueArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  let c1: C1[1] = C1[1]"
    "  fun get(f: C1[1]) =>\n"
    "    true\n"
    "  new create() =>\n"
    "    get(c1)";

  TEST_COMPILE(src);
}

TEST_F(VDTTest, FunctionCallWithSubtypeOfValueDependentTypeMismatch)
{
  const char* src =
    "trait C1[n: U32]\n"
    "class C2 is C1[3]\n"
    "class C3\n"
    "  let c2: C2 = C2\n"
    "\n"
    "  fun get(c1: C1[4]) =>\n"
    "    true\n"
    "\n"
    "  new create() =>\n"
    "    get(c2)\n";

  TEST_ERROR(src);
}

TEST_F(VDTTest, FunctionCallWithSubtypeOfValueDependentType)
{
  const char* src =
    "trait C1[n: U32]\n"
    "class C2 is C1[4]\n"
    "class C3\n"
    "  let c2: C2 = C2\n"
    "\n"
    "  fun get(c1: C1[4]) =>\n"
    "    true\n"
    "\n"
    "  new create() =>\n"
    "    get(c2)\n";

  TEST_COMPILE(src);
}

TEST_F(VDTTest, ValueDependentTypeOfParameterTypeAgree)
{
  const char* src =
    "trait T1[A, n: A]\n"
    "class C1 is T1[U32, 4]\n"
    "class C2 is T1[Bool, true]\n";
  TEST_COMPILE(src);
}

TEST_F(VDTTest, ValueDependentTypeNotOfParameterType)
{
  const char* src =
    "trait T1[A, n: A]\n"
    "class C1 is T1[U32, true]\n"
    "class C2 is T1[Bool, 4]\n";
  TEST_ERROR(src);
}

TEST_F(VDTTest, ValueDependentTypeOfParameterType)
{
  const char* src =
    "trait T1[A: (U32 | U64), n: A]\n"
    "  fun get(): A => n\n"
    "\n"
    "class C1 is T1[U32, 4]\n"
    "\n"
    "class C2\n"
    "  let c : C1 = C1\n"
    "  new create() =>\n"
    "    let u : U32 = c.get()\n";
  TEST_COMPILE(src);
}

TEST_F(VDTTest, ValueDependentTypeOfParameterTypeBadReturn)
{
  const char* src =
    "trait T1[A: (U32 | U64), n: A]\n"
    "  fun get(): A => n\n"
    "\n"
    "class C1 is T1[U32, 4]\n"
    "\n"
    "actor A1\n"
    "  let c : C1 = C1\n"
    "\n"
    "  new create() =>\n"
    "    let u : U64 = c.get()\n";
  TEST_ERROR(src);
}

TEST_F(VDTTest, TypeForThisForValueDependentType)
{
  const char* src =
    "class C1[n: U32]\n"
    "  new create() => this\n";
  TEST_COMPILE(src);
}

TEST_F(VDTTest, CreateValueDependentInClassConstructor)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  new create() =>\n"
    "    let c1: C1[4] = C1[4]\n";
  TEST_COMPILE(src);
}

TEST_F(VDTTest, CreateValueDependentMisimatchInClassConstructor)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  new create() =>\n"
    "    let c1: C1[4] = C1[70]\n";
  TEST_ERROR(src);
}

TEST_F(VDTTest, CreateValueDependentTypeInActorConstructor)
{
  const char* src =
    "class C1[n: U32]\n"
    "  fun get(): U32 => n\n"
    "\n"
    "actor A\n"
    "  new create() =>\n"
    "    let c: C1[2] = C1[2]\n";
  TEST_COMPILE(src);
}

TEST_F(VDTTest, FunctionCallWithTypeMismatchValueArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  fun get(f: C1[2]) =>\n"
    "    true\n"
    "  new create() =>\n"
    "    let c1: C1[1] ref = C1[1]\n"
    "    get(c1)";

  TEST_ERROR(src);
}

TEST_F(VDTTest, FunctionCallWithTypeValueArgument)
{
  const char* src =
    "class C1[n: U32]\n"
    "class C2\n"
    "  fun get(f: C1[1]) =>\n"
    "    true\n"
    "  new create() =>\n"
    "    let c1: C1[1] ref = C1[1]\n"
    "    get(c1)";

  TEST_COMPILE(src);
}

/*
this test breaks

slightly more advnaced test the call should be typed with the resulting type
TEST_F(VDTTest, TestU64OutsideU32Constraint)
{
  const char* src =
    "trait T1[n: U32]\n"
    "class C1 is T1[let n = U64(2)]";

  TEST_ERROR(src);
}
*/
