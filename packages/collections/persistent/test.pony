use "ponytest"
use mut = "collections"
use "random"

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    run_tests(test)

  fun tag run_tests(test: PonyTest) =>
    test(_TestListPrepend)
    test(_TestListFrom)
    test(_TestListApply)
    test(_TestListValues)
    test(_TestListConcat)
    test(_TestListMap)
    test(_TestListFlatMap)
    test(_TestListFilter)
    test(_TestListFold)
    test(_TestListEveryExists)
    test(_TestListPartition)
    test(_TestListDrop)
    test(_TestListDropWhile)
    test(_TestListTake)
    test(_TestListTakeWhile)
    test(_TestMap)
    test(_TestMapVsMap)

class iso _TestListPrepend is UnitTest
  fun name(): String => "collections/persistent/List/prepend()"

  fun apply(h: TestHelper) ? =>
    let a = Lists[U32].empty()
    let b = Lists[U32].cons(1, Lists[U32].empty())
    let c = Lists[U32].cons(2, b)
    let d = c.prepend(3)
    let e = a.prepend(10)

    h.assert_eq[USize](a.size(), 0)
    h.assert_eq[USize](b.size(), 1)
    h.assert_eq[USize](c.size(), 2)
    h.assert_eq[USize](d.size(), 3)
    h.assert_eq[USize](e.size(), 1)

    h.assert_eq[U32](b.head(), 1)
    h.assert_eq[USize](b.tail().size(), 0)
    h.assert_eq[U32](c.head(), 2)
    h.assert_eq[USize](c.tail().size(), 1)
    h.assert_eq[U32](d.head(), 3)
    h.assert_eq[USize](d.tail().size(), 2)
    h.assert_eq[U32](e.head(), 10)
    h.assert_eq[USize](e.tail().size(), 0)

    true

class iso _TestListFrom is UnitTest
  fun name(): String => "collections/persistent/Lists/from()"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1, 2, 3])
    h.assert_eq[USize](l1.size(), 3)
    h.assert_eq[U32](l1.head(), 1)

    true

class iso _TestListApply is UnitTest
  fun name(): String => "collections/persistent/List/apply()"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1, 2, 3])
    h.assert_eq[U32](l1(0), 1)
    h.assert_eq[U32](l1(1), 2)
    h.assert_eq[U32](l1(2), 3)
    h.assert_error(lambda()(l1)? => l1(3) end)
    h.assert_error(lambda()(l1)? => l1(4) end)

    let l2 = Lists[U32].empty()
    h.assert_error(lambda()(l2)? => l2(0) end)

class iso _TestListValues is UnitTest
  fun name(): String => "collections/persistent/List/values()"

  fun apply(h: TestHelper) ? =>
    let iter = Lists[U32]([1, 2, 3]).values()
    h.assert_true(iter.has_next())
    h.assert_eq[U32](iter.next(), 1)
    h.assert_true(iter.has_next())
    h.assert_eq[U32](iter.next(), 2)
    h.assert_true(iter.has_next())
    h.assert_eq[U32](iter.next(), 3)
    h.assert_false(iter.has_next())
    h.assert_false(try iter.next(); true else false end)
    h.assert_false(iter.has_next())
    h.assert_false(try iter.next(); true else false end)

class iso _TestListConcat is UnitTest
  fun name(): String => "collections/persistent/List/concat()"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1, 2, 3])
    let l2 = Lists[U32]([4, 5, 6])
    let l3 = l1.concat(l2)
    let l4 = l3.reverse()
    h.assert_eq[USize](l3.size(), 6)
    h.assert_true(Lists[U32].eq(l3, Lists[U32]([1,2,3,4,5,6])))
    h.assert_true(Lists[U32].eq(l4, Lists[U32]([6,5,4,3,2,1])))

    let l5 = Lists[U32].empty()
    let l6 = l5.reverse()
    let l7 = l6.concat(l1)
    h.assert_eq[USize](l6.size(), 0)
    h.assert_true(Lists[U32].eq(l7, Lists[U32]([1,2,3])))

    let l8 = Lists[U32]([1])
    let l9 = l8.reverse()
    h.assert_true(Lists[U32].eq(l9, Lists[U32]([1])))

    true

class iso _TestListMap is UnitTest
  fun name(): String => "collections/persistent/Lists/map()"

  fun apply(h: TestHelper) ? =>
    let l5 = Lists[U32]([1, 2, 3]).map[U32](lambda(x: U32): U32 => x * 2 end)
    h.assert_true(Lists[U32].eq(l5, Lists[U32]([2,4,6])))

    true

class iso _TestListFlatMap is UnitTest
  fun name(): String => "collections/persistent/Lists/flat_map()"

  fun apply(h: TestHelper) ? =>
    let f = lambda(x: U32): List[U32] => Lists[U32]([x - 1, x, x + 1]) end
    let l6 = Lists[U32]([2, 5, 8]).flat_map[U32](f)
    h.assert_true(Lists[U32].eq(l6, Lists[U32]([1,2,3,4,5,6,7,8,9])))

    true

class iso _TestListFilter is UnitTest
  fun name(): String => "collections/persistent/Lists/filter()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => (x % 2) == 0 end
    let l7 = Lists[U32]([1,2,3,4,5,6,7,8]).filter(is_even)
    h.assert_true(Lists[U32].eq(l7, Lists[U32]([2,4,6,8])))

    true

class iso _TestListFold is UnitTest
  fun name(): String => "collections/persistent/Lists/fold()"

  fun apply(h: TestHelper) ? =>
    let add = lambda(acc: U32, x: U32): U32 => acc + x end
    let value = Lists[U32]([1,2,3]).fold[U32](add, 0)
    h.assert_eq[U32](value, 6)

    let doubleAndPrepend =
      lambda(acc: List[U32], x: U32): List[U32] => acc.prepend(x * 2) end
    let l8 =
      Lists[U32]([1,2,3]).fold[List[U32]](doubleAndPrepend, Lists[U32].empty())
    h.assert_true(Lists[U32].eq(l8, Lists[U32]([6,4,2])))

    true

class iso _TestListEveryExists is UnitTest
  fun name(): String => "collections/persistent/Lists/every()exists()"

  fun apply(h: TestHelper) =>
    let is_even = lambda(x: U32): Bool => (x % 2) == 0 end
    let l9 = Lists[U32]([4,2,10])
    let l10 = Lists[U32]([1,1,3])
    let l11 = Lists[U32]([1,1,2])
    let l12 = Lists[U32]([2,2,3])
    let l13 = Lists[U32].empty()
    h.assert_eq[Bool](l9.every(is_even), true)
    h.assert_eq[Bool](l10.every(is_even), false)
    h.assert_eq[Bool](l11.every(is_even), false)
    h.assert_eq[Bool](l12.every(is_even), false)
    h.assert_eq[Bool](l13.every(is_even), true)
    h.assert_eq[Bool](l9.exists(is_even), true)
    h.assert_eq[Bool](l10.exists(is_even), false)
    h.assert_eq[Bool](l11.exists(is_even), true)
    h.assert_eq[Bool](l12.exists(is_even), true)
    h.assert_eq[Bool](l13.exists(is_even), false)

    true

class iso _TestListPartition is UnitTest
  fun name(): String => "collections/persistent/Lists/partition()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => (x % 2) == 0 end
    let l = Lists[U32]([1,2,3,4,5,6])
    (let hits, let misses) = l.partition(is_even)
    h.assert_true(Lists[U32].eq(hits, Lists[U32]([2,4,6])))
    h.assert_true(Lists[U32].eq(misses, Lists[U32]([1,3,5])))

    true

class iso _TestListDrop is UnitTest
  fun name(): String => "collections/persistent/List/drop()"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a","b","c","d","e"])
    let l2 = Lists[U32]([1,2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.drop(3), Lists[String](["d","e"])))
    h.assert_true(Lists[U32].eq(l2.drop(3), Lists[U32].empty()))
    h.assert_true(Lists[String].eq(empty.drop(3), Lists[String].empty()))
    true

class iso _TestListDropWhile is UnitTest
  fun name(): String => "collections/persistent/List/drop_while()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => (x % 2) == 0 end
    let l = Lists[U32]([4,2,6,1,3,4,6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.drop_while(is_even), Lists[U32]([1,3,4,6])))
    h.assert_true(Lists[U32].eq(empty.drop_while(is_even), Lists[U32].empty()))
    true

class iso _TestListTake is UnitTest
  fun name(): String => "collections/persistent/List/take()"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a","b","c","d","e"])
    let l2 = Lists[U32]([1,2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.take(3), Lists[String](["a","b","c"])))
    h.assert_true(Lists[U32].eq(l2.take(3), Lists[U32]([1,2])))
    h.assert_true(Lists[String].eq(empty.take(3), Lists[String].empty()))
    true

class iso _TestListTakeWhile is UnitTest
  fun name(): String => "collections/persistent/List/take_while()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => (x % 2) == 0 end
    let l = Lists[U32]([4,2,6,1,3,4,6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.take_while(is_even), Lists[U32]([4,2,6])))
    h.assert_true(Lists[U32].eq(empty.take_while(is_even), Lists[U32].empty()))

    true

class iso _TestMap is UnitTest
  fun name(): String => "collections/persistent/Map"

  fun apply(h: TestHelper) =>
    let m1: Map[String,U32] = Maps.empty[String,U32]()
    h.assert_error(lambda()(m1)? => m1("a") end)
    let s1 = m1.size()
    h.assert_eq[USize](s1, 0)

    try
      let m2 = m1("a") = 5
      let m3 = m2("b") = 10
      let m4 = m3("a") = 4
      let m5 = m4("c") = 0
      h.assert_eq[U32](m2("a"), 5)
      h.assert_eq[U32](m3("b"), 10)
      h.assert_eq[U32](m4("a"), 4)
      h.assert_eq[U32](m5("c"), 0)
    else
      h.complete(false)
    end

    try
      let m6 = Maps.from[String,U32]([("a", 2), ("b", 3), ("d", 4), ("e", 5)])
      let m7 = m6("a") = 10
      h.assert_eq[U32](m6("a"), 2)
      h.assert_eq[U32](m6("b"), 3)
      h.assert_eq[U32](m6("d"), 4)
      h.assert_eq[U32](m6("e"), 5)
      h.assert_eq[U32](m7("a"), 10)
      h.assert_eq[U32](m7("b"), 3)
      h.assert_eq[U32](m7("a"), 10)
      let m8 = m7.remove("a")
      h.assert_error(lambda()(m8 = m8)? => m8("a") end)
      h.assert_eq[U32](m8("b"), 3)
      h.assert_eq[U32](m8("d"), 4)
      h.assert_eq[U32](m8("e"), 5)
      let m9 = m7.remove("e")
      h.assert_error(lambda()(m9 = m9)? => m9("e") end)
      h.assert_eq[U32](m9("b"), 3)
      h.assert_eq[U32](m9("d"), 4)
      let m10 = m9.remove("b").remove("d")
      h.assert_error(lambda()(m10 = m10)? => m10("b") end)
      h.assert_error(lambda()(m10 = m10)? => m10("d") end)
    else
      h.complete(false)
    end

    true

class iso _TestMapVsMap is UnitTest
  fun name(): String => "collections/persistent/mapvsmap"

  fun apply(h: TestHelper) ? =>
    let keys: USize = 300
    var p_map: Map[String, U64] = Maps.empty[String, U64]()
    let m_map: mut.Map[String, U64] = mut.Map[String, U64](keys)
    let r = MT

    var count: USize = 0
    while count < keys do
      let k: String = count.string()
      let v = r.next()
      p_map = p_map(k) = v
      m_map(k) = v
      count = count + 1
    end

    h.assert_eq[USize](m_map.size(), keys)
    h.assert_eq[USize](p_map.size(), keys)

    for (k, v) in m_map.pairs() do
      h.assert_eq[U64](p_map(k), v)
    end

    true
