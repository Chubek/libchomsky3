# Chapter 5: Pattern Matching and Pattern Keys

Pattern matching uses `match`, `when`, and `otherwise`.

```cpp
auto kind = dsl::match(
  dsl::when<1>([](auto, auto){ return "one"; }),
  dsl::otherwise([](auto, auto){ return "other"; })
);
```

## String patterns

`dsl::pattern<"...">` supports a compact regex-like subset: `.`, character classes (`[a-z]`), `*`, `+`, and anchored whole-string matching.

```cpp
auto classify = dsl::match(
  dsl::when<dsl::pattern<"[0-9]+">>([](auto, auto){ return "int"; }),
  dsl::otherwise([](auto, auto){ return "non-int"; })
);
```

Use this for lightweight lexical categorization, not full regex engines.
