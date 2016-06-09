# Syntax
## Value-Dependent Types
* similar to generics, but using a leading lowercase identifier for parameters
* one must also provide a type constraint on the value
* example: class C1[n: U32]
* type arguments must be compile-time expressions and must be prefixed with '#'
* literals may be provided without a '#' for syntatic convinience
* example: C1[2] or C1[# 2]

## Compile-Time Expressions
* '# \<postfix\>' is the bnf that is used
* putting a '#' in front of an expression tells the compiler to try to evaluate the expression

# Rules for compile-time expressions

* Primitive literal values such as integers, floating-point values, boolean and strings are compile-time values.
* Basic (e.g. add, and) methods defined for these primitives are compile-time expressions.
* let constants which have been assigned compile-time expressions are usable within compile-time expressions.
* var variables declared within compile-time expressions are usable within the same compile-time expression.
* Only classes defined without using var fields may be instantiated as compile-time objects.
* Compile-time expressions must be recoverable to val capability values.
* Field lookups on compile-time objects are compile-time expressions.
* Methods built using these rules, and using compile-time values for arguments, can be used within compile-time expressions.
* If the result of a compile-time expression is an error then compilation fails
* Actors and behaviours cannot be used in compile-time expressions.

# Examples
```pony
actor Main
  new create(env: Env) =>
    let x: U32 = #((1 + 3) * 18)
```

```pony
actor Main
  new create(env: Env) =>
    let x: String = #("Hello" + " " + "World" + "!")
```

```pony
actor Main
  new create(env: Env) =>
    let x: U32 = # 42
    let y: U32 = # x + 12
```

```pony
actor Main
  fun fac(n: U32): U32 =>
    var i: U32 = 0
    var result: U32 = 1
    while (i = i + 1) <= n do
      result = result * i
    else
      result
    end

  new create(env: Env) =>
    let x = # fac(10)
```

```pony
class C1
  let x: U32
  new create(x': U32) => x = x'
  fun apply(): U32 => x * x

actor Main
  new create(env: Env) =>
    let c = # C1(42)
    let x = # c()
```

```pony
class C1[n: U32]
  fun apply(): U32 => n

actor Main
  new create(env: Env) =>
    let c = C1[2]
    let x = c()
```

```pony
class C1
  let x: U32
  new create(x': U32) => x = x'

class C2[c: C1 val]
  fun apply(): U32 => c.x

actor Main
  new create(env: Env) =>
    let c = C2[# C1(42)]
    let x = c()
```

```pony
primitive Assert
  fun apply(b: Bool) ? => if not b then error end

actor Main
  new create(env: Env) =>
    # Assert(U32(10) < U32(2))
```
