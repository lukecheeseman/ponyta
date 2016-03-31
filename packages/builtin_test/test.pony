"""
# Builtin Tests

This package contains the unit tests for the `builtin` package. These are here
so `builtin` doesn't have to depend on the `PonyTest` package.
"""

use "ponytest"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestAbs)
    test(_TestStringRunes)
    test(_TestIntToString)
    test(_TestFloatToString)
    test(_TestStringToBool)
    test(_TestStringToFloat)
    test(_TestStringToU8)
    test(_TestStringToI8)
    test(_TestStringToIntLarge)
    test(_TestStringLstrip)
    test(_TestStringRstrip)
    test(_TestStringStrip)
    test(_TestStringRemove)
    test(_TestStringSubstring)
    test(_TestStringCut)
    test(_TestStringReplace)
    test(_TestStringSplit)
    test(_TestStringJoin)
    test(_TestStringCount)
    test(_TestStringCompare)
    test(_TestSpecialValuesF32)
    test(_TestSpecialValuesF64)
    test(_TestArraySlice)
    test(_TestArrayInsert)
    test(_TestMath128)
    test(_TestDivMod)
    test(_TestMaybePointer)
    test(_TestValtrace)
    test(_TestCCallback)
    test(_TestVector)


class iso _TestAbs is UnitTest
  """
  Test abs function
  """
  fun name(): String => "builtin/Int.abs"

  fun apply(h: TestHelper) =>
    h.assert_eq[U8](128, I8(-128).abs())
    h.assert_eq[U8](3,   I8(-3).abs())
    h.assert_eq[U8](14,  I8(14).abs())
    h.assert_eq[U8](0,   I8(0).abs())

    h.assert_eq[U16](128,   I16(-128).abs())
    h.assert_eq[U16](32768, I16(-32768).abs())
    h.assert_eq[U16](3,     I16(-3).abs())
    h.assert_eq[U16](27,    I16(-27).abs())
    h.assert_eq[U16](0,     I16(0).abs())

    h.assert_eq[U32](128,        I32(-128).abs())
    h.assert_eq[U32](32768,      I32(-32768).abs())
    h.assert_eq[U32](2147483648, I32(-2147483648).abs())
    h.assert_eq[U32](3,          I32(-3).abs())
    h.assert_eq[U32](124,        I32(-124).abs())
    h.assert_eq[U32](0,          I32(0).abs())

    h.assert_eq[U64](128,        I64(-128).abs())
    h.assert_eq[U64](32768,      I64(-32768).abs())
    h.assert_eq[U64](2147483648, I64(-2147483648).abs())
    h.assert_eq[U64](128,        I64(-128).abs())
    h.assert_eq[U64](3,          I64(-3).abs())
    h.assert_eq[U64](124,        I64(-124).abs())
    h.assert_eq[U64](0,          I64(0).abs())

    h.assert_eq[U128](128,        I128(-128).abs())
    h.assert_eq[U128](32768,      I128(-32768).abs())
    h.assert_eq[U128](2147483648, I128(-2147483648).abs())
    h.assert_eq[U128](128,        I128(-128).abs())
    h.assert_eq[U128](3,          I128(-3).abs())
    h.assert_eq[U128](124,        I128(-124).abs())
    h.assert_eq[U128](0,          I128(0).abs())


class iso _TestStringRunes is UnitTest
  """
  Test iterating over the unicode codepoints in a string.
  """
  fun name(): String => "builtin/String.runes"

  fun apply(h: TestHelper) ? =>
    let s = "\u16ddx\ufb04"
    let expect = [as U32: 0x16dd, 'x', 0xfb04]
    let result = Array[U32]

    for c in s.runes() do
      result.push(c)
    end

    h.assert_eq[USize](expect.size(), result.size())

    for i in Range(0, expect.size()) do
      h.assert_eq[U32](expect(i), result(i))
    end

    true


class iso _TestIntToString is UnitTest
  """
  Test converting integers to strings.
  """
  fun name(): String => "builtin/U32.string"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("0", U32(0).string())
    h.assert_eq[String]("3", U32(3).string())
    h.assert_eq[String]("1234", U32(1234).string())
    h.assert_eq[String]("003",
      U32(3).string(FormatSettingsInt.set_precision(3)))
    h.assert_eq[String]("  3",
      U32(3).string(FormatSettingsInt.set_width(3)))
    h.assert_eq[String]("  003",
      U32(3).string(FormatSettingsInt.set_precision(3).set_width(5)))


class iso _TestFloatToString is UnitTest
  """
  Test converting floats to strings.
  """
  fun name(): String => "builtin/Float.string"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("0", F32(0).string())
    h.assert_eq[String]("-0.35", F32(-3.5e-1).string())
    h.assert_eq[String]("123.125", F32(123.125).string())
    h.assert_eq[String]("0", F64(0).string())
    h.assert_eq[String]("-0.35", F64(-3.5e-1).string())
    h.assert_eq[String]("123.125", F64(123.125).string())


class iso _TestStringToBool is UnitTest
  """
  Test converting strings to Bools.
  """
  fun name(): String => "builtin/String.bool"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[Bool](false, "false".bool())
    h.assert_eq[Bool](false, "FALSE".bool())
    h.assert_eq[Bool](true, "true".bool())
    h.assert_eq[Bool](true, "TRUE".bool())

    h.assert_error(lambda()? => "bogus".bool() end)


class iso _TestStringToFloat is UnitTest
  """
  Test converting strings to floats.
  """
  fun name(): String => "builtin/String.float"

  fun apply(h: TestHelper) =>
    h.assert_eq[F32](4.125, "4.125".f32())
    h.assert_eq[F64](4.125, "4.125".f64())

    h.assert_eq[F32](-4.125e-3, "-4.125e-3".f32())
    h.assert_eq[F64](-4.125e-3, "-4.125e-3".f64())


class iso _TestStringToU8 is UnitTest
  """
  Test converting strings to U8s.
  """
  fun name(): String => "builtin/String.u8"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[U8](0, "0".u8())
    h.assert_eq[U8](123, "123".u8())
    h.assert_eq[U8](123, "0123".u8())
    h.assert_eq[U8](89, "089".u8())

    h.assert_error(lambda()? => "300".u8() end, "U8 300")
    h.assert_error(lambda()? => "30L".u8() end, "U8 30L")
    h.assert_error(lambda()? => "-10".u8() end, "U8 -10")

    h.assert_eq[U8](16, "0x10".u8())
    h.assert_eq[U8](31, "0x1F".u8())
    h.assert_eq[U8](31, "0x1f".u8())
    h.assert_eq[U8](31, "0X1F".u8())
    h.assert_eq[U8](2, "0b10".u8())
    h.assert_eq[U8](2, "0B10".u8())
    h.assert_eq[U8](0x8A, "0b1000_1010".u8())

    h.assert_error(lambda()? => "1F".u8() end, "U8 1F")
    h.assert_error(lambda()? => "0x".u8() end, "U8 0x")
    h.assert_error(lambda()? => "0b3".u8() end, "U8 0b3")
    h.assert_error(lambda()? => "0d4".u8() end, "U8 0d4")


class iso _TestStringToI8 is UnitTest
  """
  Test converting strings to I8s.
  """
  fun name(): String => "builtin/String.i8"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[I8](0, "0".i8())
    h.assert_eq[I8](123, "123".i8())
    h.assert_eq[I8](123, "0123".i8())
    h.assert_eq[I8](89, "089".i8())
    h.assert_eq[I8](-10, "-10".i8())

    h.assert_error(lambda()? => "200".i8() end, "I8 200")
    h.assert_error(lambda()? => "30L".i8() end, "I8 30L")

    h.assert_eq[I8](16, "0x10".i8())
    h.assert_eq[I8](31, "0x1F".i8())
    h.assert_eq[I8](31, "0x1f".i8())
    h.assert_eq[I8](31, "0X1F".i8())
    h.assert_eq[I8](2, "0b10".i8())
    h.assert_eq[I8](2, "0B10".i8())
    h.assert_eq[I8](0x4A, "0b100_1010".i8())
    h.assert_eq[I8](-0x4A, "-0b100_1010".i8())

    h.assert_error(lambda()? => "1F".i8() end, "U8 1F")
    h.assert_error(lambda()? => "0x".i8() end, "U8 0x")
    h.assert_error(lambda()? => "0b3".i8() end, "U8 0b3")
    h.assert_error(lambda()? => "0d4".i8() end, "U8 0d4")


class iso _TestStringToIntLarge is UnitTest
  """
  Test converting strings to I* and U* types bigger than 8 bit.
  """
  fun name(): String => "builtin/String.toint"

  fun apply(h: TestHelper) ? =>
    h.assert_eq[U16](0, "0".u16())
    h.assert_eq[U16](123, "123".u16())
    h.assert_error(lambda()? => "-10".u16() end, "U16 -10")
    h.assert_error(lambda()? => "65536".u16() end, "U16 65536")
    h.assert_error(lambda()? => "30L".u16() end, "U16 30L")

    h.assert_eq[I16](0, "0".i16())
    h.assert_eq[I16](123, "123".i16())
    h.assert_eq[I16](-10, "-10".i16())
    h.assert_error(lambda()? => "65536".i16() end, "I16 65536")
    h.assert_error(lambda()? => "30L".i16() end, "I16 30L")

    h.assert_eq[U32](0, "0".u32())
    h.assert_eq[U32](123, "123".u32())
    h.assert_error(lambda()? => "-10".u32() end, "U32 -10")
    h.assert_error(lambda()? => "30L".u32() end, "U32 30L")

    h.assert_eq[I32](0, "0".i32())
    h.assert_eq[I32](123, "123".i32())
    h.assert_eq[I32](-10, "-10".i32())
    h.assert_error(lambda()? => "30L".i32() end, "I32 30L")

    h.assert_eq[U64](0, "0".u64())
    h.assert_eq[U64](123, "123".u64())
    h.assert_error(lambda()? => "-10".u64() end, "U64 -10")
    h.assert_error(lambda()? => "30L".u64() end, "U64 30L")

    h.assert_eq[I64](0, "0".i64())
    h.assert_eq[I64](123, "123".i64())
    h.assert_eq[I64](-10, "-10".i64())
    h.assert_error(lambda()? => "30L".i64() end, "I64 30L")

    h.assert_eq[U128](0, "0".u128())
    h.assert_eq[U128](123, "123".u128())
    h.assert_error(lambda()? => "-10".u128() end, "U128 -10")
    h.assert_error(lambda()? => "30L".u128() end, "U128 30L")

    h.assert_eq[I128](0, "0".i128())
    h.assert_eq[I128](123, "123".i128())
    h.assert_eq[I128](-10, "-10".i128())
    h.assert_error(lambda()? => "30L".i128() end, "I128 30L")


class iso _TestStringLstrip is UnitTest
  """
  Test stripping leading characters from a string.
  """
  fun name(): String => "builtin/String.lstrip"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover "foobar".clone().lstrip("foo") end, "bar")
    h.assert_eq[String](recover "fooooooobar".clone().lstrip("foo") end, "bar")
    h.assert_eq[String](recover "   foobar".clone().lstrip() end, "foobar")
    h.assert_eq[String](recover "  foobar  ".clone().lstrip() end, "foobar  ")


class iso _TestStringRstrip is UnitTest
  """
  Test stripping trailing characters from a string.
  """
  fun name(): String => "builtin/String.rstrip"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover "foobar".clone().rstrip("bar") end, "foo")
    h.assert_eq[String](recover "foobaaaaaar".clone().rstrip("bar") end, "foo")
    h.assert_eq[String](recover "foobar  ".clone().rstrip() end, "foobar")
    h.assert_eq[String](recover "  foobar  ".clone().rstrip() end, "  foobar")


class iso _TestStringStrip is UnitTest
  """
  Test stripping leading and trailing characters from a string.
  """
  fun name(): String => "builtin/String.strip"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](recover "  foobar  ".clone().strip() end, "foobar")
    h.assert_eq[String](recover "barfoobar".clone().strip("bar") end, "foo")
    h.assert_eq[String](recover "foobarfoo".clone().strip("foo") end, "bar")
    h.assert_eq[String](recover "foobarfoo".clone().strip("bar") end,
      "foobarfoo")


class iso _TestStringRemove is UnitTest
  """
  Test removing characters from a string (independent of leading or trailing).
  """
  fun name(): String => "builtin/String.remove"

  fun apply(h: TestHelper) =>
    let s1 = recover "  foo   bar  ".clone() end
    let s2 = recover "barfoobar".clone() end
    let s3 = recover "f-o-o-b-a-r!".clone() end
    let s4 = recover "f-o-o-b-a-r!".clone() end

    let r1 = s1.remove(" ")
    let r2 = s2.remove("foo")
    let r3 = s3.remove("-")
    let r4 = s4.remove("-!")

    h.assert_eq[USize](r1, 7)
    h.assert_eq[USize](r2, 1)
    h.assert_eq[USize](r3, 5)
    h.assert_eq[USize](r4, 0)

    h.assert_eq[String](consume s1, "foobar")
    h.assert_eq[String](consume s2, "barbar")
    h.assert_eq[String](consume s3, "foobar!")
    h.assert_eq[String](consume s4, "f-o-o-b-a-r!")


class iso _TestStringSubstring is UnitTest
  """
  Test copying range of characters.
  """
  fun name(): String => "builtin/String.substring"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("3456", "0123456".substring(3, 99))

    h.assert_eq[String]("345", "0123456".substring(3, 6))
    h.assert_eq[String]("3456", "0123456".substring(3, 7))
    h.assert_eq[String]("3456", "0123456".substring(3))
    h.assert_eq[String]("345", "0123456".substring(3, -1))


class iso _TestStringCut is UnitTest
  """
  Test cutting part of a string
  """
  fun name(): String => "builtin/String.cut"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("01236", "0123456".cut(4, 6))
    h.assert_eq[String]("0123", "0123456".cut(4, 7))
    h.assert_eq[String]("0123", "0123456".cut(4))


class iso _TestSpecialValuesF32 is UnitTest
  """
  Test whether a F32 is infinite or NaN.
  """
  fun name(): String => "builtin/F32.finite"

  fun apply(h: TestHelper) =>
    // 1
    h.assert_true(F32(1.0).finite())
    h.assert_false(F32(1.0).nan())

    // -1
    h.assert_true(F32(-1.0).finite())
    h.assert_false(F32(-1.0).nan())

    // Infinity
    h.assert_false(F32(1.0 / 0.0).finite())
    h.assert_false(F32(1.0 / 0.0).nan())

    // - infinity
    h.assert_false(F32(-1.0 / 0.0).finite())
    h.assert_false(F32(-1.0 / 0.0).nan())

    // NaN
    h.assert_false(F32(0.0 / 0.0).finite())
    h.assert_true(F32(0.0 / 0.0).nan())


class iso _TestSpecialValuesF64 is UnitTest
  """
  Test whether a F64 is infinite or NaN.
  """
  fun name(): String => "builtin/F64.finite"

  fun apply(h: TestHelper) =>
    // 1
    h.assert_true(F64(1.0).finite())
    h.assert_false(F64(1.0).nan())

    // -1
    h.assert_true(F64(-1.0).finite())
    h.assert_false(F64(-1.0).nan())

    // Infinity
    h.assert_false(F64(1.0 / 0.0).finite())
    h.assert_false(F64(1.0 / 0.0).nan())

    // - infinity
    h.assert_false(F64(-1.0 / 0.0).finite())
    h.assert_false(F64(-1.0 / 0.0).nan())

    // NaN
    h.assert_false(F64(0.0 / 0.0).finite())
    h.assert_true(F64(0.0 / 0.0).nan())


class iso _TestStringReplace is UnitTest
  """
  Test String.replace
  """
  fun name(): String => "builtin/String.replace"

  fun apply(h: TestHelper) =>
    let s = String.append("this is a robbery, this is a stickup")
    s.replace("is a", "is not a")
    h.assert_eq[String box](s, "this is not a robbery, this is not a stickup")


class iso _TestStringSplit is UnitTest
  """
  Test String.split
  """
  fun name(): String => "builtin/String.split"

  fun apply(h: TestHelper) ? =>
    var r = "1 2 3  4".split()
    h.assert_eq[USize](r.size(), 5)
    h.assert_eq[String](r(0), "1")
    h.assert_eq[String](r(1), "2")
    h.assert_eq[String](r(2), "3")
    h.assert_eq[String](r(3), "")
    h.assert_eq[String](r(4), "4")

    r = "1 2 3  4".split(where n = 3)
    h.assert_eq[USize](r.size(), 3)
    h.assert_eq[String](r(0), "1")
    h.assert_eq[String](r(1), "2")
    h.assert_eq[String](r(2), "3  4")


class iso _TestStringJoin is UnitTest
  """
  Test String.join
  """
  fun name(): String => "builtin/String.join"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("_".join(["zomg"]), "zomg")
    h.assert_eq[String]("_".join(["hi", "there"]), "hi_there")
    h.assert_eq[String](" ".join(["1", "", "2", ""]), "1  2 ")
    h.assert_eq[String](" ".join([as Stringable: U32(1), U32(4)]), "1 4")
    h.assert_eq[String](" ".join(Array[String]), "")


class iso _TestStringCount is UnitTest
  """
  Test String.count
  """
  fun name(): String => "builtin/String.count"

  fun apply(h: TestHelper) =>
    let testString: String = "testString"
    h.assert_eq[USize](testString.count(testString), 1)
    h.assert_eq[USize](testString.count("testString"), 1)
    h.assert_eq[USize]("testString".count(testString), 1)
    h.assert_eq[USize]("".count("zomg"), 0)
    h.assert_eq[USize]("zomg".count(""), 0)
    h.assert_eq[USize]("azomg".count("zomg"), 1)
    h.assert_eq[USize]("zomga".count("zomg"), 1)
    h.assert_eq[USize]("atatat".count("at"), 3)
    h.assert_eq[USize]("atatbat".count("at"), 3)
    h.assert_eq[USize]("atata".count("ata"), 1)
    h.assert_eq[USize]("tttt".count("tt"), 2)

class iso _TestStringCompare is UnitTest
  """
  Test comparing strings.
  """
  fun name(): String => "builtin/String.compare"

  fun apply(h: TestHelper) =>
    h.assert_eq[Compare](Equal, "foo".compare("foo"))
    h.assert_eq[Compare](Greater, "foo".compare("bar"))
    h.assert_eq[Compare](Less, "bar".compare("foo"))
    
    h.assert_eq[Compare](Less, "abc".compare("bc"))

    h.assert_eq[Compare](Equal, "foo".compare_sub("foo", 3))
    h.assert_eq[Compare](Greater, "foo".compare_sub("bar", 3))
    h.assert_eq[Compare](Less, "bar".compare_sub("foo", 3))

    h.assert_eq[Compare](Equal, "foobar".compare_sub("foo", 3))
    h.assert_eq[Compare](Greater, "foobar".compare_sub("foo", 4))

    h.assert_eq[Compare](Less, "foobar".compare_sub("oob", 3, 0))
    h.assert_eq[Compare](Equal, "foobar".compare_sub("oob", 3, 1))

    h.assert_eq[Compare](
      Equal, "ab".compare_sub("ab", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Greater, "ab".compare_sub("Ab", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Greater, "ab".compare_sub("aB", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Greater, "ab".compare_sub("AB", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Less, "AB".compare_sub("ab", 2 where ignore_case = false))
    h.assert_eq[Compare](
      Equal, "AB".compare_sub("AB", 2 where ignore_case = false))

    h.assert_eq[Compare](
      Equal, "ab".compare_sub("ab", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "ab".compare_sub("Ab", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "ab".compare_sub("aB", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "ab".compare_sub("AB", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "AB".compare_sub("ab", 2 where ignore_case = true))
    h.assert_eq[Compare](
      Equal, "AB".compare_sub("AB", 2 where ignore_case = true))

    h.assert_eq[Compare](Equal, "foobar".compare_sub("bar", 2, -2, -2))

    h.assert_eq[Compare](
      Greater, "one".compare("four"), "\"one\" > \"four\"")
    h.assert_eq[Compare](
      Less,    "four".compare("one"), "\"four\" < \"one\"")
    h.assert_eq[Compare](
      Equal,   "one".compare("one"), "\"one\" == \"one\"")
    h.assert_eq[Compare](
      Less,    "abc".compare("abcd"), "\"abc\" < \"abcd\"")
    h.assert_eq[Compare](
      Greater, "abcd".compare("abc"), "\"abcd\" > \"abc\"")
    h.assert_eq[Compare](
      Equal,   "abcd".compare_sub("abc", 3), "\"abcx\" == \"abc\"")
    h.assert_eq[Compare](
      Equal,   "abc".compare_sub("abcd", 3), "\"abc\" == \"abcx\"")
    h.assert_eq[Compare](
      Equal,   "abc".compare_sub("abc", 4), "\"abc\" == \"abc\"")
    h.assert_eq[Compare](
      Equal,   "abc".compare_sub("babc", 4, 1, 2), "\"xbc\" == \"xxbc\"")


class iso _TestArraySlice is UnitTest
  """
  Test slicing arrays.
  """
  fun name(): String => "builtin/Array.slice"

  fun apply(h: TestHelper) ? =>
    let a = ["one", "two", "three", "four", "five"]

    let b = a.slice(1, 4)
    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[String]("two", b(0))
    h.assert_eq[String]("three", b(1))
    h.assert_eq[String]("four", b(2))

    let c = a.slice(0, 5, 2)
    h.assert_eq[USize](c.size(), 3)
    h.assert_eq[String]("one", c(0))
    h.assert_eq[String]("three", c(1))
    h.assert_eq[String]("five", c(2))

    let d = a.reverse()
    h.assert_eq[USize](d.size(), 5)
    h.assert_eq[String]("five", d(0))
    h.assert_eq[String]("four", d(1))
    h.assert_eq[String]("three", d(2))
    h.assert_eq[String]("two", d(3))
    h.assert_eq[String]("one", d(4))

    let e = a.permute(Reverse(4, 0, 2))
    h.assert_eq[USize](e.size(), 3)
    h.assert_eq[String]("five", e(0))
    h.assert_eq[String]("three", e(1))
    h.assert_eq[String]("one", e(2))


class iso _TestArrayInsert is UnitTest
  """
  Test inserting new element into array
  """
  fun name(): String => "builtin/Array.insert"

  fun apply(h: TestHelper) ? =>
    let a = ["one", "three"]
    a.insert(0, "zero")
    h.assert_array_eq[String](["zero", "one", "three"], a)

    let b = ["one", "three"]
    b.insert(1, "two")
    h.assert_array_eq[String](["one", "two", "three"], b)

    let c = ["one", "three"]
    c.insert(2, "four")
    h.assert_array_eq[String](["one", "three", "four"], c)

    h.assert_error(lambda()? => ["one", "three"].insert(3, "invalid") end)


class iso _TestMath128 is UnitTest
  """
  Test 128 bit integer math.
  """
  fun name(): String => "builtin/Math128"

  fun apply(h: TestHelper) =>
    h.assert_eq[F64](0, U128(0).f64())
    h.assert_eq[F64](1, U128(1).f64())
    h.assert_eq[F64](1e10, U128(10_000_000_000).f64())
    h.assert_eq[F64](1e20, U128(100_000_000_000_000_000_000).f64())

    h.assert_eq[F64](0, I128(0).f64())
    h.assert_eq[F64](1, I128(1).f64())
    h.assert_eq[F64](1e10, I128(10_000_000_000).f64())
    h.assert_eq[F64](1e20, I128(100_000_000_000_000_000_000).f64())

    h.assert_eq[F64](-1, I128(-1).f64())
    h.assert_eq[F64](-1e10, I128(-10_000_000_000).f64())
    h.assert_eq[F64](-1e20, I128(-100_000_000_000_000_000_000).f64())

    h.assert_eq[I128](0, 0 * 3)
    h.assert_eq[I128](8, 2 * 4)
    h.assert_eq[I128](1_000_000_000_000, 1_000_000 * 1_000_000)
    h.assert_eq[I128](100_000_000_000_000_000_000,
      10_000_000_000 * 10_000_000_000)

    h.assert_eq[I128](-8, -2 * 4)
    h.assert_eq[I128](-1_000_000_000_000, -1_000_000 * 1_000_000)
    h.assert_eq[I128](-100_000_000_000_000_000_000,
      -10_000_000_000 * 10_000_000_000)

    h.assert_eq[I128](8, -2 * -4)
    h.assert_eq[I128](1_000_000_000_000, -1_000_000 * -1_000_000)
    h.assert_eq[I128](100_000_000_000_000_000_000,
      -10_000_000_000 * -10_000_000_000)

    // Stop compiler moaning about dividing by constant 0.
    var uzero = U128(0)
    var izero = I128(0)

    h.assert_eq[U128](0, 100 / uzero)
    h.assert_eq[U128](2, 8 / 4)
    h.assert_eq[U128](1_000_000, 1_000_000_000_000 / 1_000_000)
    h.assert_eq[U128](10_000_000_000,
      100_000_000_000_000_000_000 / 10_000_000_000)

    h.assert_eq[I128](0, 100 / izero)
    h.assert_eq[I128](2, 8 / 4)
    h.assert_eq[I128](1_000_000, 1_000_000_000_000 / 1_000_000)
    h.assert_eq[I128](10_000_000_000,
      100_000_000_000_000_000_000 / 10_000_000_000)

    h.assert_eq[I128](0, -100 / izero)
    h.assert_eq[I128](-2, -8 / 4)
    h.assert_eq[I128](-1_000_000, -1_000_000_000_000 / 1_000_000)
    h.assert_eq[I128](-10_000_000_000,
      -100_000_000_000_000_000_000 / 10_000_000_000)

    h.assert_eq[I128](0, -100 / -izero)
    h.assert_eq[I128](2, -8 / -4)
    h.assert_eq[I128](1_000_000, -1_000_000_000_000 / -1_000_000)
    h.assert_eq[I128](10_000_000_000,
      -100_000_000_000_000_000_000 / -10_000_000_000)

    h.assert_eq[U128](0, 100 % uzero)
    h.assert_eq[U128](5, 13 % 8)
    h.assert_eq[U128](28, 40_000_000_028 % 10_000_000_000)

    h.assert_eq[I128](0, 100 % izero)
    h.assert_eq[I128](5, 13 % 8)
    h.assert_eq[I128](28, 40_000_000_028 % 10_000_000_000)

    h.assert_eq[I128](-5, -13 % 8)
    h.assert_eq[I128](-28, -40_000_000_028 % 10_000_000_000)

    h.assert_eq[I128](5, 13 % -8)
    h.assert_eq[I128](28, 40_000_000_028 % -10_000_000_000)

    h.assert_eq[I128](-5, -13 % -8)
    h.assert_eq[I128](-28, -40_000_000_028 % -10_000_000_000)


class iso _TestDivMod is UnitTest
  """
  Test divmod on various bit widths.
  """
  fun name(): String => "builtin/DivMod"

  fun apply(h: TestHelper) =>
    h.assert_eq[I8](5, 13 % 8)
    h.assert_eq[I8](-5, -13 % 8)
    h.assert_eq[I8](5, 13 % -8)
    h.assert_eq[I8](-5, -13 % -8)

    h.assert_eq[I16](5, 13 % 8)
    h.assert_eq[I16](-5, -13 % 8)
    h.assert_eq[I16](5, 13 % -8)
    h.assert_eq[I16](-5, -13 % -8)

    h.assert_eq[I32](5, 13 % 8)
    h.assert_eq[I32](-5, -13 % 8)
    h.assert_eq[I32](5, 13 % -8)
    h.assert_eq[I32](-5, -13 % -8)

    h.assert_eq[I64](5, 13 % 8)
    h.assert_eq[I64](-5, -13 % 8)
    h.assert_eq[I64](5, 13 % -8)
    h.assert_eq[I64](-5, -13 % -8)


struct _TestStruct
  var i: U32 = 0

class iso _TestMaybePointer is UnitTest
  """
  Test the MaybePointer type.
  """
  fun name(): String => "builtin/MaybePointer"

  fun apply(h: TestHelper) ? =>
    let a = MaybePointer[_TestStruct].none()
    h.assert_true(a.is_none())

    h.assert_error(lambda()(a)? => let from_a = a() end)

    let s = _TestStruct
    s.i = 7

    let b = MaybePointer[_TestStruct](s)
    h.assert_false(b.is_none())

    let from_b = b()
    h.assert_eq[U32](s.i, from_b.i)


class Callback
  fun apply(value: I32): I32 =>
    value * 2

class iso _TestCCallback is UnitTest
  """
  Test C callbacks.
  """
  fun name(): String => "builtin/CCallback"

  fun apply(h: TestHelper) =>
    let cb: Callback = Callback
    let r = @pony_test_callback[I32](cb, addressof cb.apply, I32(3))
    h.assert_eq[I32](6, r)

class iso _TestVector is UnitTest
  fun name(): String => "builtin/Vector"

  fun apply(h: TestHelper) ? =>
    //let vector1 = Vector[4, String]
    let vector1 = Vector[String, 4]
    vector1.update(0, "A")
    vector1.update(1, "B")
    vector1.update(2, "C")
    vector1.update(3, "D")

    h.assert_eq[USize](vector1.size(), 4)

    let array1 = ["A", "B", "C", "D"]
    var i: USize = 0
    while i < vector1.size() do
      h.assert_eq[String](vector1(i), array1(i))
      i = i + 1
    end
