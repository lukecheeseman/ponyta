"""
# Base64 package

The Base64 package contains support for doing Base64 binary-to-text encodings.
We currently have support 3 encodings: PEM, MIME and URL.

To learn more about Base64, we suggest you check out the
[wikipedia entry](https://en.wikipedia.org/wiki/Base64).

## Example code

```pony
use "encode/base64"

actor Main
  new create(env: Env) =>
    env.out.print(Base64.encode("foobar"))
    try
      env.out.print(Base64.decode[String iso]("Zm9vYmFy"))
    end
```
"""
use "collections"
use "assert"

primitive Base64
  fun encode_pem(data: ReadSeq[U8]): String iso^ =>
    """
    Encode for PEM (RFC 1421).
    """
    encode(data, '+', '/', '=', 64)

  fun encode_mime(data: ReadSeq[U8]): String iso^ =>
    """
    Encode for MIME (RFC 2045).
    """
    encode(data, '+', '/', '=', 76)

  fun encode_url[A: Seq[U8] iso = String iso](data: ReadSeq[U8],
    pad: Bool = false): A^
  =>
    """
    Encode for URLs (RFC 4648). Padding characters are stripped by default.
    """
    let c: U8 = if pad then '=' else 0 end
    encode[A](data, '-', '_', c)

  fun encode[A: Seq[U8] iso = String iso](data: ReadSeq[U8], at62: U8 = '+',
    at63: U8 = '/', pad: U8 = '=', linelen: USize = 0,
    linesep: String = "\r\n"): A^
  =>
    """
    Configurable encoding. The defaults are for RFC 4648.
    """
    let len = ((data.size() + 2) / 3) * 4
    let out = recover A(len) end
    let lineblocks = linelen / 4

    var srclen = data.size()
    var blocks = USize(0)
    var i = USize(0)

    try
      while srclen >= 3 do
        let in1 = data(i)
        let in2 = data(i + 1)
        let in3 = data(i + 2)

        let out1 = in1 >> 2
        let out2 = ((in1 and 0x03) << 4) + (in2 >> 4)
        let out3 = ((in2 and 0x0f) << 2) + (in3 >> 6)
        let out4 = in3 and 0x3f

        out.push(_enc_byte(out1, at62, at63))
        out.push(_enc_byte(out2, at62, at63))
        out.push(_enc_byte(out3, at62, at63))
        out.push(_enc_byte(out4, at62, at63))

        i = i + 3
        blocks = blocks + 1
        srclen = srclen - 3

        if (lineblocks > 0) and (blocks == lineblocks) then
          out.append(linesep)
          blocks = 0
        end
      end

      if srclen >= 1 then
        let in1 = data(i)
        let in2 = if srclen == 2 then data(i + 1) else 0 end

        let out1 = in1 >> 2
        let out2 = ((in1 and 0x03) << 4) + (in2 >> 4)
        let out3 = (in2 and 0x0f) << 2

        out.push(_enc_byte(out1, at62, at63))
        out.push(_enc_byte(out2, at62, at63))

        if srclen == 2 then
          out.push(_enc_byte(out3, at62, at63))
        else
          out.push(pad)
        end

        out.push(pad)
      end

      if lineblocks > 0 then
        out.append(linesep)
      end
    else
      out.clear()
    end

    out

  fun decode_url[A: Seq[U8] iso = Array[U8] iso](data: ReadSeq[U8]): A^ ? =>
    """
    Decode for URLs (RFC 4648).
    """
    decode[A](data, '-', '_')

  fun decode[A: Seq[U8] iso = Array[U8] iso](data: ReadSeq[U8], at62: U8 = '+',
    at63: U8 = '/', pad: U8 = '='): A^ ?
  =>
    """
    Configurable decoding. The defaults are for RFC 4648. Missing padding is
    not an error. Non-base64 data, other than whitespace (which can appear at
    any time), is an error.
    """
    let len = (data.size() * 4) / 3
    let out = recover A(len) end

    var state = U8(0)
    var input = U8(0)
    var output = U8(0)

    for i in Range(0, data.size()) do
      input = data(i)

      let value = match input
      | ' ' | '\t' | '\r' | '\n' => continue
      | pad => break
      | at62 => 62
      | at63 => 63
      | if (input >= 'A') and (input <= 'Z') =>
        (input - 'A')
      | if (input >= 'a') and (input <= 'z') =>
        ((input - 'a') + 26)
      | if (input >= '0') and (input <= '9') =>
        ((input - '0') + 52)
      else
        error
      end

      match state
      | 0 =>
        output = value << 2
        state = 1
      | 1 =>
        out.push(output or (value >> 4))
        output = (value and 0x0f) << 4
        state = 2
      | 2 =>
        out.push(output or (value >> 2))
        output = (value and 0x03) << 6
        state = 3
      | 3 =>
        out.push(output or value)
        state = 0
      else
        error
      end
    end

    if output != 0 then
      Fact(input != pad)

      match state
      | 1 | 2 => out.push(output)
      end
    end

    out

  fun _enc_byte(i: U8, at62: U8, at63: U8): U8 ? =>
    """
    Encode a single byte.
    """
    match i
    | 62 => at62
    | 63 => at63
    | if i < 26 => 'A' + i
    | if i < 52 => ('a' - 26) + i
    | if i < 62 => ('0' - 52) + i
    else
      error
    end
