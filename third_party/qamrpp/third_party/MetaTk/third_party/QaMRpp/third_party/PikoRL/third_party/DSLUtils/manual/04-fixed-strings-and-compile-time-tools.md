# Chapter 4: Fixed Strings and Compile-Time Keys

`dsl::FixedString<N>` stores compile-time string data and offers `.view()` for runtime use.

## Where it appears

- AST tags: `leaf<"num">`, `node<"mul">`
- Rewrite rule IDs: `rule<"const-fold">(...)`
- Literal suffix IDs: `lit<"_km">(...)`
- Parser production IDs: `production<"expr">(...)`

## Practical advice

- Treat NTTP strings as stable identifiers.
- Use short lowercase tags (`"num"`, `"add"`, `"neg"`) for AST and rule names.
- Keep parser labels human-friendly for diagnostics.
