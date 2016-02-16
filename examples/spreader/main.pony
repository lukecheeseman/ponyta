actor Spreader
  var _env: (Env | None)
  var _count: U64 = 0

  var _parent: (Spreader | None) = None
  var _result: U64 = 0
  var _received: U64 = 0

  new create(env: Env) =>
    _env = env

    _count = try env.args(1).u64() else 10 end

    if _count > 1 then
      spawn_child()
      spawn_child()
    else
      env.out.print("1 actor")
    end

  new spread(parent: Spreader, count: U64) =>
    _env = None

    if count == 1 then
      parent.result(1)
    else
      _parent = parent
      _count = count

      spawn_child()
      spawn_child()
    end

  be result(i: U64) =>
    _received = _received + 1
    _result = _result + i

    if _received == 2 then
      match (_parent, _env)
      | (let p: Spreader, _) =>
        p.result(_result + 1)
      | (None, let e: Env) =>
        e.out.print((_result + 1).string() + " actors")
      end
    end

  fun spawn_child() =>
    Spreader.spread(this, _count - 1)

actor Main
  new create(env: Env) =>
    Spreader(env)
