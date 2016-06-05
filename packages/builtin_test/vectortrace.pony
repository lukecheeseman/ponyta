use "ponytest"

class iso _TestVectorTrace is UnitTest
  """
  Test val trace optimisation
  """
  fun name(): String => "builtin/Vectortrace"

  fun apply(h: TestHelper) =>
    _VectorTrace.one(h)
    h.long_test(2_000_000_000) // 2 second timeout


actor _VectorTrace

  be three(h: TestHelper, v: Vector[String, 3] iso) =>
    @pony_triggergc[None](this)
    try
      h.assert_eq[String]("wombat", v(0))
      h.assert_eq[String]("aardvark", v(1))
      h.assert_eq[String]("meerkat", v(2))
    else
      h.fail()
    end
    h.complete(true)

  be two(h: TestHelper, s1: String, s2: String, s3: String) =>
    @pony_triggergc[None](this)
    try
      let v = recover Vector[String, 3].init([s1, s2, s3].values()) end
      _VectorTrace.three(h, consume v)
    else
      h.fail()
    end

  be one(h: TestHelper) =>
    @pony_triggergc[None](this)
    let s1 = recover String.append("wombat") end
    let s2 = recover String.append("aardvark") end
    let s3 = recover String.append("meerkat") end
    _VectorTrace.two(h, consume s1, consume s2, consume s3)
