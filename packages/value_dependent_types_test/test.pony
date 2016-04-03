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
