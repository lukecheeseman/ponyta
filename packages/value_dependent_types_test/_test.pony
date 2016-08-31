"""
# Value Dependent Types Tests

"""
use "ponytest"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestU32Value)
    test(_TestU32AddValue)
    test(_TestU64Arithmetic)
    test(_TestU64Bitwise)
    test(_TestIntegerToIntegerTypeCast)
    test(_TestNegation)
    test(_TestChainedArithemtic)
    test(_TestIntegerEquivalence)
    test(_TestBool)
    test(_TestFunctionCall)
    test(_TestFunctionCallNamedArgs)
    test(_TestCompileTimeObjectField)
    test(_TestCompileTimeObjectMethod)
    test(_TestCompileTimeDependentObject)
    test(_TestCompileTimeObjectEmbeddedField)
    test(_TestTraitValueType)
    test(_TestCompileTimeVector)
    test(_TestCompileTimeVariable)
    test(_TestCompileTimeObjectValueDependentType)

class C1[n: U32]
  fun apply(): U32 => n

class iso _TestU32Value is UnitTest
  """
  Test retrieve U32 value
  """
  fun name(): String => "VDT/U32Value"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](C1[42](), 42)

class iso _TestU64Arithmetic is UnitTest
  """
  Test compile time U64 arithmertic
  """
  fun name(): String => "VDT/U64Arithmetic"

  fun apply(h: TestHelper) =>
    h.assert_eq[U64](#(42 + 8), 42 + 8)
    h.assert_eq[U64](#(42 * 8), 42 * 8)
    h.assert_eq[U64](#(42 / 8), 42 / 8)

class iso _TestU32AddValue is UnitTest
  """
  Test retrieve U32 Addition value
  """
  fun name(): String => "VDT/U32AddValue"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](C1[#(91 + 5)](), 96)

class iso _TestU64Bitwise is UnitTest

  fun name(): String => "VDT/U64Bitwise"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#(3823890482 and 123931), 3823890482 and 123931)
    h.assert_eq[U32](#(3823890482 or 123931), 3823890482 or 123931)

class iso _TestChainedArithemtic is UnitTest

  fun name(): String => "VDT/ChainedArithmetic"

  fun m1[x: I32, y: I32, z: I32](): I32 =>
    #(-x + (y - z))

  fun m2(x: I32, y: I32, z: I32): I32 =>
    -x + (y - z)

  fun m3[x: I32, y: I32, z: I32](): I32 =>
    #((-x + y) - z)

  fun m4(x: I32, y: I32, z: I32): I32 =>
    (-x + y) - z

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#(-2 + (1 - 9)), -2 + (1 - 9))
    h.assert_eq[I32](#(-2 + (1 - 9)), -2 + (1 - 9))
    h.assert_eq[I32](m1[#(-2),1,9](), m2(-2,1,9))
    h.assert_eq[I32](m3[#(-2),1,9](), m4(-2,1,9))
    h.assert_eq[I32](#(-2 + 1), -2 + 1)
    h.assert_eq[I32](#(1 + -2), 1 + -2)
    h.assert_eq[I32](#(-1 + 2), -1 + 2)
    h.assert_eq[I32](#(2 + -1), 2 + -1)
    h.assert_eq[I32](#(-1 - -1), -1 - -1)
    h.assert_eq[I32](#(-1 - 1), -1 - 1)

class iso _TestNegation is UnitTest

  fun name(): String => "VDT/Negation"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#(-1), -1)
    h.assert_eq[I32](#(-1), -1)
    h.assert_eq[I32](#(42 - 78), 42 - 78)

class iso _TestIntegerEquivalence is UnitTest

  fun name(): String => "VDT/IntegerEquivalence"

  fun apply(h: TestHelper) =>
    h.assert_true(#(U32(1) == U32(1)))
    h.assert_false(#(U32(1) == U32(7)))
    h.assert_true(#(I32(-1) == I32(-1)))
    h.assert_false(#(I32(-1) == I32(-7)))
    h.assert_true(#(I32(1) == -I32(-1)))

class iso _TestIntegerToIntegerTypeCast is UnitTest
  """
  Test casting one integer type to another as
  compile time expression
  """

  fun name(): String => "VDT/IntegerCast"

  fun apply(h: TestHelper) =>
    h.assert_eq[I8](#(U32(4).i8()), 4)
    h.assert_eq[I16](#(U32(4).i16()), 4)
    h.assert_eq[I32](#(U32(4).i32()), 4)
    h.assert_eq[I64](#(U32(4).i64()), 4)
    h.assert_eq[I128](#(U32(4).i128()), 4)
    h.assert_eq[ILong](#(U32(4).ilong()), 4)
    h.assert_eq[ISize](#(U32(4).isize()), 4)
    h.assert_eq[U8](#(U32(4).u8()), 4)
    h.assert_eq[U16](#(U32(4).u16()), 4)
    h.assert_eq[U32](#(U32(4).u32()), 4)
    h.assert_eq[U64](#(U32(4).u64()), 4)
    h.assert_eq[U128](#(U32(4).u128()), 4)
    h.assert_eq[ULong](#(U32(4).ulong()), 4)
    h.assert_eq[USize](#(U32(4).usize()), 4)

    h.assert_eq[I8](#(I128(73).i8()), 73)
    h.assert_eq[I16](#(I128(73).i16()), 73)
    h.assert_eq[I32](#(I128(73).i32()), 73)
    h.assert_eq[I64](#(I128(73).i64()), 73)
    h.assert_eq[I128](#(I128(73).i128()), 73)
    h.assert_eq[ILong](#(I128(73).ilong()), 73)
    h.assert_eq[ISize](#(I128(73).isize()), 73)
    h.assert_eq[U8](#(I128(73).u8()), 73)
    h.assert_eq[U16](#(I128(73).u16()), 73)
    h.assert_eq[U32](#(I128(73).u32()), 73)
    h.assert_eq[U64](#(I128(73).u64()), 73)
    h.assert_eq[U128](#(I128(73).u128()), 73)
    h.assert_eq[ULong](#(I128(73).ulong()), 73)
    h.assert_eq[USize](#(I128(73).usize()), 73)

class iso _TestBool is UnitTest

  fun name(): String => "VDT/Bool"

  fun bool_or[b1: Bool, b2: Bool](): Bool =>
    #(b1 or b2)

  fun bool_and[b1: Bool, b2: Bool](): Bool =>
    #(b1 and b2)

  fun apply(h: TestHelper) =>
    h.assert_false(bool_or[false, false]())
    h.assert_true(bool_or[false, true]())
    h.assert_true(bool_or[true, false]())
    h.assert_true(bool_or[true, true]())

    h.assert_false(bool_and[false, false]())
    h.assert_false(bool_and[false, true]())
    h.assert_false(bool_and[true, false]())
    h.assert_true(bool_and[true, true]())

class iso _TestFunctionCall is UnitTest

  fun name(): String => "VDT/fib"

  fun fib(n: U32): U32 =>
    if n == 0 then
      0
    elseif n <= 2 then
      1
    else
      fib(n - 2) + fib(n - 1)
    end

   fun apply(h: TestHelper) =>
      h.assert_eq[U32](#fib(1), fib(1))
      h.assert_eq[U32](#fib(8), fib(8))
      h.assert_eq[U32](#fib(20), fib(20))

class iso _TestFunctionCallNamedArgs is UnitTest

  fun name(): String => "VDT/named"

  fun foo(x: U32, y: U32, z:U32 = 4): U32 => (x * y) + z

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#foo(1, 2, 3), foo(1, 2, 3))
    h.assert_eq[U32](#foo(1 where y=2, z=3), foo(1 where y=2, z=3))
    h.assert_eq[U32](#foo(1 where z=2, y=3), foo(1 where z=2, y=3))
    h.assert_eq[U32](#foo(1 where y=3), foo(1 where y=3))

class C2
  let x: U32

  new val create(x': U32) => x = x'

  fun apply(): U32 => x + x + x

class C3[c: C2 val]
  new val create() => true
  fun apply(): C2 val => c

class C4
  embed c: C2 val

  new val create(x: U32) => c = C2(x)

class iso _TestCompileTimeObjectField is UnitTest

  fun name(): String => "VDT/CompileTimeObjectField"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#(C2(48).x), C2(48).x)
    h.assert_eq[U32](#(C2(48)).x, C2(48).x)

class iso _TestCompileTimeObjectMethod is UnitTest

  fun name(): String => "VDT/CompileTimeObjectMethod"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#(C2(48)()), C2(48)())
    h.assert_eq[U32](#(C2(48))(), C2(48)())

class iso _TestCompileTimeDependentObject is UnitTest

  fun name(): String => "VDT/CompileTimeDependentObject"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#(C3[#C2(48)]().x), 48)
    let c = #C3[#C2(471)]
    let x = c().x
    h.assert_eq[U32](x, 471)

class iso _TestCompileTimeObjectEmbeddedField is UnitTest

  fun name(): String => "VDT/CompileTimeObjectiEmbeddedField"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](#(C4(12).c.x), 12)
    let c1 = #(C4(982))
    h.assert_eq[U32](c1.c.x, 982)
    let c2 = #(C4(78).c)
    h.assert_eq[U32](c2.x, 78)
    let x = #(C4(2123).c.x)
    h.assert_eq[U32](x, 2123)

trait T1
  fun foo(): U32
class C5 is T1
  fun foo(): U32 => 2
class C6 is T1
  fun foo(): U32 => 19
class C7[t: T1 val]
  fun apply(): U32 => t.foo()

class iso _TestTraitValueType is UnitTest

  fun name(): String => "VDT/TraitValueType"

  fun apply(h: TestHelper) =>
    h.assert_eq[U32](C7[# C5].apply(), 2)
    h.assert_eq[U32](C7[# C6].apply(), 19)

class iso _TestCompileTimeVector is UnitTest

  fun name(): String => "VDT/CompileTimeVector"

  fun apply(h: TestHelper) =>
    let v1: Vector[U32, 4] = {1, 2, 3, 4}
    let v2: Vector[U32, 4] val = # {1, 2, 3, 4}
    try
      h.assert_eq[USize](v1.size(), v2.size())
      for i in Range(0, 4) do
        h.assert_eq[U32](v1(i), v2(i))
      end

      h.assert_eq[U32](v2(0), # v2(0))
      h.assert_eq[U32](v2(1), # v2(1))
      h.assert_eq[U32](v2(2), # v2(2))
      h.assert_eq[U32](v2(3), # v2(3))
    else
      h.fail()
    end

class iso _TestCompileTimeVariable is UnitTest

  fun name(): String => "VDT/CompileTimeVariable"

  fun apply(h: TestHelper) =>
    let x: U32 = # (1 + 2)
    let y: U32 = # (x * 2)
    h.assert_eq[U32]((1+2)*2, # y)

    let c = # C2(79)
    h.assert_eq[U32](c.x, # c.x)
    //FIXME: the following isn't returning the correct value
    // it's grabbed the earlier C2 with 48 as the field
    //h.assert_eq[U32](c(), # c())

class C8[b: Bool]
class C9[c: C8[true] val]

class iso _TestCompileTimeObjectValueDependentType is UnitTest

  fun name(): String => "VDT/CompileTimeObjectValueDependentType"

  fun apply(h: TestHelper) =>
    let c1 = # C8[true]
    let c2 = C9[# c1 ]
