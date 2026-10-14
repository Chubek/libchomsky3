# Chapter 9: Expression Templates

`dsl::ExprTemplates` adds lazy arithmetic tree operators to your DSL type.

```cpp
struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  double v{};
  double expr_value() const { return v; }
};
```

Expressions like `a + b * c` build `BinExpr`/`UnaryExpr` nodes. Evaluation happens only at `.eval()`.

Use this when you want fused/deferrable expression assembly semantics rather than eager arithmetic.
