# Rules for compile-time expressions

* Primitive literal values such as integers, floating-point values, boolean and strings are compile-time values.
* Basic (e.g. add, and) methods defined for these primitives are compile-time expressions.
* let constants which have been assigned compile-time expressions are usable within compile-time expressions.
* var variables declared within compile-time expressions are usable within compile-time expressions.
* Only classes defined without using var fields may be instantiated as compile-time objects.
* Compile-time expressions must be recoverable to val capability values.
* Field lookups on compile-time objects are compile-time expressions.
* Methods built using these rules, and using compile-time values for arguments, can be used within compile-time expressions.
* Actors and behaviours cannot be used in compile-time expressions.

# Examples

```pony
class C1[n: U32]
  fun apply(): U32 => n

actor Main
  new create(env: Env) =>
    let c = C1[2]
    let x = c()
```
