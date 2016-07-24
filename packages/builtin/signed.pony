primitive I8 is _SignedInteger[I8, U8]
  new create(value: I8 = 0) => value
  fun tag from[A: (Number & Real[A] val)](a: A): I8 => a.i8()

  fun tag min_value(): I8 => -0x80
  fun tag max_value(): I8 => 0x7F

  fun abs(): U8 => if this < 0 then (-this).u8() else this.u8() end
  fun bswap(): I8 => this
  fun popcount(): I8 => @"llvm.ctpop.i8"[I8](this)
  fun clz(): I8 => @"llvm.ctlz.i8"[I8](this, false)
  fun ctz(): I8 => @"llvm.cttz.i8"[I8](this, false)
  fun bitwidth(): I8 => 8

  fun min(y: I8): I8 => if this < y then this else y end
  fun max(y: I8): I8 => if this > y then this else y end

  fun addc(y: I8): (I8, Bool) =>
    @"llvm.sadd.with.overflow.i8"[(I8, Bool)](this, y)
  fun subc(y: I8): (I8, Bool) =>
    @"llvm.ssub.with.overflow.i8"[(I8, Bool)](this, y)
  fun mulc(y: I8): (I8, Bool) =>
    @"llvm.smul.with.overflow.i8"[(I8, Bool)](this, y)

primitive I16 is _SignedInteger[I16, U16]
  new create(value: I16 = 0) => value
  fun tag from[A: (Number & Real[A] val)](a: A): I16 => a.i16()

  fun tag min_value(): I16 => -0x8000
  fun tag max_value(): I16 => 0x7FFF

  fun abs(): U16 => if this < 0 then (-this).u16() else this.u16() end
  fun bswap(): I16 => @"llvm.bswap.i16"[I16](this)
  fun popcount(): I16 => @"llvm.ctpop.i16"[I16](this)
  fun clz(): I16 => @"llvm.ctlz.i16"[I16](this, false)
  fun ctz(): I16 => @"llvm.cttz.i16"[I16](this, false)
  fun bitwidth(): I16 => 16

  fun min(y: I16): I16 => if this < y then this else y end
  fun max(y: I16): I16 => if this > y then this else y end

  fun addc(y: I16): (I16, Bool) =>
    @"llvm.sadd.with.overflow.i16"[(I16, Bool)](this, y)
  fun subc(y: I16): (I16, Bool) =>
    @"llvm.ssub.with.overflow.i16"[(I16, Bool)](this, y)
  fun mulc(y: I16): (I16, Bool) =>
    @"llvm.smul.with.overflow.i16"[(I16, Bool)](this, y)

primitive I32 is _SignedInteger[I32, U32]
  new create(value: I32 = 0) => value
  fun tag from[A: (Number & Real[A] val)](a: A): I32 => a.i32()

  fun tag min_value(): I32 => -0x8000_0000
  fun tag max_value(): I32 => 0x7FFF_FFFF

  fun abs(): U32 => if this < 0 then (-this).u32() else this.u32() end
  fun bswap(): I32 => @"llvm.bswap.i32"[I32](this)
  fun popcount(): I32 => @"llvm.ctpop.i32"[I32](this)
  fun clz(): I32 => @"llvm.ctlz.i32"[I32](this, false)
  fun ctz(): I32 => @"llvm.cttz.i32"[I32](this, false)
  fun bitwidth(): I32 => 32

  fun min(y: I32): I32 => if this < y then this else y end
  fun max(y: I32): I32 => if this > y then this else y end

  fun addc(y: I32): (I32, Bool) =>
    @"llvm.sadd.with.overflow.i32"[(I32, Bool)](this, y)
  fun subc(y: I32): (I32, Bool) =>
    @"llvm.ssub.with.overflow.i32"[(I32, Bool)](this, y)
  fun mulc(y: I32): (I32, Bool) =>
    @"llvm.smul.with.overflow.i32"[(I32, Bool)](this, y)

primitive I64 is _SignedInteger[I64, U64]
  new create(value: I64 = 0) => value
  fun tag from[A: (Number & Real[A] val)](a: A): I64 => a.i64()

  fun tag min_value(): I64 => -0x8000_0000_0000_0000
  fun tag max_value(): I64 => 0x7FFF_FFFF_FFFF_FFFF

  fun abs(): U64 => if this < 0 then (-this).u64() else this.u64() end
  fun bswap(): I64 => @"llvm.bswap.i64"[I64](this)
  fun popcount(): I64 => @"llvm.ctpop.i64"[I64](this)
  fun clz(): I64 => @"llvm.ctlz.i64"[I64](this, false)
  fun ctz(): I64 => @"llvm.cttz.i64"[I64](this, false)
  fun bitwidth(): I64 => 64

  fun min(y: I64): I64 => if this < y then this else y end
  fun max(y: I64): I64 => if this > y then this else y end

  fun addc(y: I64): (I64, Bool) =>
    @"llvm.sadd.with.overflow.i64"[(I64, Bool)](this, y)
  fun subc(y: I64): (I64, Bool) =>
    @"llvm.ssub.with.overflow.i64"[(I64, Bool)](this, y)
  fun mulc(y: I64): (I64, Bool) =>
    @"llvm.smul.with.overflow.i64"[(I64, Bool)](this, y)

primitive ILong is _SignedInteger[ILong, ULong]
  new create(value: ILong = 0) => value
  fun tag from[A: (Number & Real[A] val)](a: A): ILong => a.ilong()

  fun tag min_value(): ILong =>
    ifdef ilp32 or llp64 then
      -0x8000_0000
    else
      -0x8000_0000_0000_0000
    end

  fun tag max_value(): ILong =>
    ifdef ilp32 or llp64 then
      0x7FFF_FFFF
    else
      0x7FFF_FFFF_FFFF_FFFF
    end

  fun abs(): ULong => if this < 0 then (-this).ulong() else this.ulong() end

  fun bswap(): ILong =>
    ifdef ilp32 or llp64 then
      @"llvm.bswap.i32"[ILong](this)
    else
      @"llvm.bswap.i64"[ILong](this)
    end

  fun popcount(): ILong =>
    ifdef ilp32 or llp64 then
      @"llvm.ctpop.i32"[ILong](this)
    else
      @"llvm.ctpop.i64"[ILong](this)
    end

  fun clz(): ILong =>
    ifdef ilp32 or llp64 then
      @"llvm.ctlz.i32"[ILong](this, false)
    else
      @"llvm.ctlz.i64"[ILong](this, false)
    end

  fun ctz(): ILong =>
    ifdef ilp32 or llp64 then
      @"llvm.cttz.i32"[ILong](this, false)
    else
      @"llvm.cttz.i64"[ILong](this, false)
    end

  fun bitwidth(): ILong => ifdef ilp32 or llp64 then 32 else 64 end
  fun min(y: ILong): ILong => if this < y then this else y end
  fun max(y: ILong): ILong => if this > y then this else y end

  fun addc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.sadd.with.overflow.i32"[(ILong, Bool)](this, y)
    else
      @"llvm.sadd.with.overflow.i64"[(ILong, Bool)](this, y)
    end

  fun subc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.ssub.with.overflow.i32"[(ILong, Bool)](this, y)
    else
      @"llvm.ssub.with.overflow.i64"[(ILong, Bool)](this, y)
    end

  fun mulc(y: ILong): (ILong, Bool) =>
    ifdef ilp32 or llp64 then
      @"llvm.smul.with.overflow.i32"[(ILong, Bool)](this, y)
    else
      @"llvm.smul.with.overflow.i64"[(ILong, Bool)](this, y)
    end

primitive ISize is _SignedInteger[ISize, USize]
  new create(value: ISize = 0) => value
  fun tag from[A: (Number & Real[A] val)](a: A): ISize => a.isize()

  fun tag min_value(): ISize =>
    ifdef ilp32 then
      -0x8000_0000
    else
      -0x8000_0000_0000_0000
    end

  fun tag max_value(): ISize =>
    ifdef ilp32 then
      0x7FFF_FFFF
    else
      0x7FFF_FFFF_FFFF_FFFF
    end

  fun abs(): USize => if this < 0 then (-this).usize() else this.usize() end

  fun bswap(): ISize =>
    ifdef ilp32 then
      @"llvm.bswap.i32"[ISize](this)
    else
      @"llvm.bswap.i64"[ISize](this)
    end

  fun popcount(): ISize =>
    ifdef ilp32 then
      @"llvm.ctpop.i32"[ISize](this)
    else
      @"llvm.ctpop.i64"[ISize](this)
    end

  fun clz(): ISize =>
    ifdef ilp32 then
      @"llvm.ctlz.i32"[ISize](this, false)
    else
      @"llvm.ctlz.i64"[ISize](this, false)
    end

  fun ctz(): ISize =>
    ifdef ilp32 then
      @"llvm.cttz.i32"[ISize](this, false)
    else
      @"llvm.cttz.i64"[ISize](this, false)
    end

  fun bitwidth(): ISize => ifdef ilp32 then 32 else 64 end
  fun min(y: ISize): ISize => if this < y then this else y end
  fun max(y: ISize): ISize => if this > y then this else y end

  fun addc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      @"llvm.sadd.with.overflow.i32"[(ISize, Bool)](this, y)
    else
      @"llvm.sadd.with.overflow.i64"[(ISize, Bool)](this, y)
    end

  fun subc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      @"llvm.ssub.with.overflow.i32"[(ISize, Bool)](this, y)
    else
      @"llvm.ssub.with.overflow.i64"[(ISize, Bool)](this, y)
    end

  fun mulc(y: ISize): (ISize, Bool) =>
    ifdef ilp32 then
      @"llvm.smul.with.overflow.i32"[(ISize, Bool)](this, y)
    else
      @"llvm.smul.with.overflow.i64"[(ISize, Bool)](this, y)
    end

primitive I128 is _SignedInteger[I128, U128]
  new create(value: I128 = 0) => value
  fun tag from[A: (Number & Real[A] val)](a: A): I128 => a.i128()

  fun tag min_value(): I128 => -0x8000_0000_0000_0000_0000_0000_0000_0000
  fun tag max_value(): I128 => 0x7FFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun abs(): U128 => if this < 0 then (-this).u128() else this.u128() end
  fun bswap(): I128 => @"llvm.bswap.i128"[I128](this)
  fun popcount(): I128 => @"llvm.ctpop.i128"[I128](this)
  fun clz(): I128 => @"llvm.ctlz.i128"[I128](this, false)
  fun ctz(): I128 => @"llvm.cttz.i128"[I128](this, false)
  fun bitwidth(): I128 => 128
  fun min(y: I128): I128 => if this < y then this else y end
  fun max(y: I128): I128 => if this > y then this else y end
  fun hash(): U64 => ((this.u128() >> 64).u64() xor this.u64()).hash()

  fun string(
    fmt: FormatSettings[FormatInt, PrefixNumber] = FormatDefaultNumber)
    : String iso^
  =>
    _ToString._u128(abs().u128(), this < 0, fmt)

  fun mul(y: I128): I128 =>
    (u128() * y.u128()).i128()

  fun divmod(y: I128): (I128, I128) =>
    ifdef native128 then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var num: I128 = if this >= 0 then this else -this end
      var den: I128 = if y >= 0 then y else -y end

      (let q, let r) = num.u128().divmod(den.u128())
      (var q', var r') = (q.i128(), r.i128())

      if this < 0 then
        r' = -r'

        if y > 0 then
          q' = -q'
        end
      elseif y < 0 then
        q' = -q'
      end

      (q', r')
    end

  fun div(y: I128): I128 =>
    ifdef native128 then
      this / y
    else
      (let q, let r) = divmod(y)
      q
    end

  fun mod(y: I128): I128 =>
    ifdef native128 then
      this % y
    else
      (let q, let r) = divmod(y)
      r
    end

  fun f32(): F32 =>
    f64().f32()

  fun f64(): F64 =>
    if this < 0 then
      -(-u128()).f64()
    else
      u128().f64()
    end

type Signed is (I8 | I16 | I32 | I64 | I128 | ILong | ISize)
