# Chapter 7: AST Model

`dsl::ASTNode` is a value type with:

- `tag()`
- `is_leaf()`
- `value()`
- `children()` / `child(i)`
- `dump()`

Build trees with:

```cpp
auto tree = dsl::node<"add">(
  dsl::leaf<"num">(1),
  dsl::leaf<"num">(2)
);
```

`dump()` emits S-expression style output. Prefer this for quick debugging and snapshots in tests.
