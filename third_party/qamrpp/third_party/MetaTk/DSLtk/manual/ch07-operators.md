# Chapter 7: The Operators Feature: Predicate Composition

## Motivation

A predicate is a callable that maps a value to a Boolean. In isolation, predicates are trivial — a lambda, a function pointer, a small functor. The difficulty appears when the rules a program must enforce are combinations of many such checks: a value must be positive *and* even, or it must satisfy one of several alternative shape requirements, or it must fail a check in order to be rejected. Writing each composite rule by hand produces repetitive, error-prone lambdas that are difficult to read and difficult to test in isolation.

The Operators feature provides a uniform wrapper, `dsl::Predicate<F>`, that turns any callable into a value with proper Boolean algebra. The three overloaded operators `&`, `|`, and `!` build new predicates from existing ones, so a complex rule can be expressed as an expression of named sub-rules. The result is closer to a small specification language embedded in C++ than to a chain of nested `if` statements.

This chapter describes the wrapper, its three operators, the `dsl::predicate` factory, and the `dsl::Operators` feature tag that exposes the same machinery through a DSL-qualified `make_pred` factory. It covers short-circuit semantics, value semantics, `constexpr` evaluability, operator precedence, and a series of worked validators. Chapter 5 introduced feature tags and the mixin mechanism; Chapter 6 covered the Pipeline feature, which is frequently combined with Operators in the same DSL definition.

## The Predicate Wrapper

The central type of the feature is `dsl::Predicate<F>`, a thin struct parameterized on the callable type `F`. It holds a single member, `fn`, of type `F`, and exposes the three composition operators. The wrapper does not own any additional state, perform any allocation, or impose any policy on what `F` may capture. Its only contract is that `F` is callable with the value passed to `operator()` and that the result is convertible to `bool`.

The constructor is `explicit` and `constexpr`. Being explicit prevents accidental implicit construction from an arbitrary callable, which is important because the operators would otherwise be selected against bare lambdas in surprising contexts. Being `constexpr` allows a predicate to be constructed in a constant-expression context, which becomes significant when composing rules at compile time. The member `fn` is move-constructed from the argument, so even stateful functors are transferred cheaply.

The call operator is a template on the argument type, taking a forwarding reference `T&& v`. It forwards `v` into the stored callable and returns the Boolean result. Because the call operator is itself `const` and `constexpr`, a `Predicate` is an immutable function object that can be evaluated in a constant expression. The forwarding preserves value category where the underlying callable allows it, which matters for move-only or non-copyable argument types.

```cpp
template <typename F> struct Predicate
{
  F fn;
  explicit constexpr Predicate (F f) : fn (std::move (f)) {}

  template <typename T>
  constexpr bool
  operator() (T &&v) const
  {
    return fn (std::forward<T> (v));
  }
  // ... operators below ...
};
```

The wrapper is, in effect, a tiny function-object adapter. Its purpose is not to add behaviour to `F` at construction time but to attach the three composition operators to a value whose type would not otherwise support them. A bare lambda has no `operator&` that means Boolean conjunction — that operator would instead perform a bitwise AND of the lambda's address with another value, which is meaningless. By wrapping the lambda in `Predicate<F>`, the overloaded operators become available.

## The predicate Factory

Constructing a `Predicate<F>` directly requires spelling out `F`, which for a lambda is an unspellable closure type. The library therefore provides a factory, `dsl::predicate`, that deduces `F` from its argument. The factory is the only sanctioned way to obtain a `Predicate` in user code; the constructor is technically accessible but impractical for lambdas.

The factory's signature is a perfect-forwarding template. The stored type is `std::decay_t<F>`, which strips references, cv-qualifiers, and array-to-pointer decay so that the member `fn` is a plain object type. This is the same convention used throughout the standard library for type-erased or wrapped callables. The argument is forwarded into the constructor, so a named functor can be moved in and a temporary lambda can be moved or copied as appropriate.

```cpp
template <typename F>
constexpr auto
predicate (F &&f)
{
  return Predicate<std::decay_t<F>>{ std::forward<F> (f) };
}
```

Because the factory returns `auto`, the result type is `Predicate<std::decay_t<F>>`, which is a concrete, named type that the compiler can print in diagnostics even though the user never writes it. In practice a predicate is almost always held in an `auto` variable, and the concrete type is irrelevant to the user. The factory is `constexpr`, so it can be used in a constant expression.

A predicate constructed from a function pointer works identically to one constructed from a lambda. The decayed type of a function name passed to the factory is a pointer-to-function, and `Predicate<bool(*)(const int&)>` is a perfectly valid instantiation. This matters when a rule reuses an existing free function rather than a freshly written lambda.

## Conjunction with operator&

The `operator&` overload implements conjunction: the composite predicate reports `true` only when both operands report `true`. The overload is a template on the right-hand operand's callable type `G`, taking a `Predicate<G> other` by value. The body constructs and returns a new `Predicate` whose stored lambda captures both callables, `a = fn` and `b = other.fn`, by copy.

```cpp
/// Conjunction: both predicates must pass.
template <typename G>
constexpr auto
operator& (Predicate<G> other) const
{
  return Predicate{ [a = fn, b = other.fn] (auto &&v)
                      { return a (v) && b (v); } };
}
```

The lambda uses the built-in `&&` operator, which short-circuits: if `a(v)` is `false`, `b(v)` is not evaluated. This is the only behaviour that makes sense for a validator, because it avoids invoking the second predicate when the first has already settled the outcome. A predicate with side effects — say, one that logs a message on a failing value — would not be invoked past the point where the result is determined.

The returned `Predicate` is a fresh, self-contained value. It captures copies of the two underlying callables, so the operands can be destroyed or reassigned without affecting the composite. There is no shared state, no reference back to the originals, and no allocation on the heap. The composite is the same kind of value as its operands: a small struct holding one or more captured callables.

Because `operator&` is `const`-qualified, conjunction can be applied to a predicate held in a `const` variable. This is consistent with the value-semantics model: a predicate is an immutable rule, and combining two rules does not modify either of them. The result is a new rule that can itself be combined further.

## Disjunction with operator|

The `operator|` overload implements disjunction. Its structure mirrors `operator&` exactly: a template on the right-hand operand's callable type, a by-value parameter, and a returned `Predicate` whose lambda captures both callables. The only difference is the body, which uses `||` instead of `&&`.

```cpp
/// Disjunction: at least one predicate must pass.
template <typename G>
constexpr auto
operator| (Predicate<G> other) const
{
  return Predicate{ [a = fn, b = other.fn] (auto &&v)
                      { return a (v) || b (v); } };
}
```

The `||` operator short-circuits on `true`: once `a(v)` returns `true`, `b(v)` is not evaluated. For a disjunctive validator this means the first sub-rule that accepts the value settles the result, and any subsequent predicates are skipped. As with conjunction, this avoids unnecessary work and avoids side effects in the skipped predicates.

Disjunction is the natural tool for expressing alternatives. A validator that accepts any of several distinct value shapes — a numeric constant in one of several ranges, a string matching one of several prefixes — is most readable when written as `rule_a | rule_b | rule_c`. Each alternative is named and testable in isolation, and the composite is a single value that can be passed to a higher-level algorithm.

The value-semantics argument from conjunction applies unchanged. The returned `Predicate` captures its operands by copy and has no link back to them. The operands may be discarded immediately after the expression, and the composite continues to behave correctly.

## Negation with operator!

The `operator!` overload is a unary prefix operator. It takes no parameters and returns a new `Predicate` whose lambda captures `a = fn` and returns `!a(v)`. Negation does not short-circuit because there is nothing to short-circuit; it is a single call to the underlying predicate followed by a Boolean not.

```cpp
/// Negation.
constexpr auto
operator!() const
{
  return Predicate{ [a = fn] (auto &&v) { return !a (v); } };
}
```

Negation is most useful as a readability device. A rule that rejects a particular shape can be written either as a positive predicate ("is not in this set") or as the negation of a positive predicate ("is in this set, then negate"). The two are semantically equivalent, but the latter often corresponds more directly to the way the rule is described in a specification. Naming the positive predicate and then negating it keeps the implementation aligned with the prose.

Because negation returns a `Predicate`, the result can be combined further. `!pred & other` negates `pred` and conjoins the result with `other`. The operands of the inner negation are evaluated first because `!` binds more tightly than `&`, which is the topic of the section on precedence below.

## The Operators Feature Tag

The feature tag `dsl::Operators` is the bridge between the standalone `dsl::predicate` factory and the DSL-qualified `make_pred` factory described in Chapter 5. The tag is an empty struct containing a nested `Mixin` template. The mixin provides a single static member function, `make_pred`, that forwards its argument to `dsl::predicate`.

```cpp
struct Operators
{
  template <typename Derived> struct Mixin
  {
    template <typename F>
    static constexpr auto
    make_pred (F &&f)
    {
      return dsl::predicate (std::forward<F> (f));
    }
  };
};
```

The mixin's `make_pred` is `static` and `constexpr`, matching the factory it delegates to. A DSL that lists `dsl::Operators` in its feature list gains `MyDSL::make_pred(...)` as a qualified name for constructing predicates. This is purely a naming convenience: the returned object is exactly the same `Predicate<std::decay_t<F>>` that `dsl::predicate` would produce. There is no per-DSL specialization, no additional behaviour, and no dependency on the `Derived` type parameter.

The reason for providing the qualified name at all is consistency with the rest of the toolkit. Other features — Pipeline, Match, Rewrite — expose their factories through the DSL's own scope, and Operators follows the same convention so that user code can be uniform across features. A reader who encounters `BasicDSL::make_pred` in a source file can locate the feature tag in the DSL declaration and understand immediately where the name comes from.

The `Derived` template parameter is unused inside the mixin. It is present because the CRTP base described in Chapter 3 instantiates every feature mixin with the derived DSL type, and the parameter must exist in the signature even when the mixin does not consume it. This keeps the feature-tag protocol uniform across all features.

## A First Composition

The basic-usage example from the distribution combines Pipeline and Operators in a single DSL. A value is threaded through two pipes, and two predicates are constructed and evaluated separately. The example illustrates that the two features coexist without interference: a DSL may list both in its feature list and use the factories of each.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};

int main() {
  auto v = BasicDSL::wrap(5) | dsl::pipe([](int x) { return x + 3; })
           | dsl::pipe([](int x) { return x * 2; });
  auto gt5 = BasicDSL::make_pred([](int x) { return x > 5; });
  auto even = BasicDSL::make_pred([](int x) { return x % 2 == 0; });
  std::cout << v << " " << (gt5(v) && even(v)) << "\n";
}
```

This example evaluates the two predicates separately and combines their results with the built-in `&&` operator. That is a legitimate use, but it does not exercise the Operators feature's composition machinery. The next section shows how the two predicates can be combined into a single composite rule before any value is tested.

## Combining Predicates into a Rule

The real power of the Operators feature is that composition happens before evaluation. Two predicates can be joined into a single value that is itself a predicate, and that composite can be passed around, stored, and applied to many values without re-stating the rule.

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct BasicDSL : dsl::DSL<BasicDSL, dsl::Operators> {};

int main() {
  auto gt5   = BasicDSL::make_pred([](int x) { return x > 5; });
  auto even  = BasicDSL::make_pred([](int x) { return x % 2 == 0; });
  auto rule  = gt5 & even;

  std::cout << rule(16) << "\n";  // 1: >5 and even
  std::cout << rule(4)  << "\n";  // 0: not >5
  std::cout << rule(7)  << "\n";  // 0: odd
}
```

The variable `rule` holds a `Predicate` whose closure type contains copies of both lambdas. Each call to `rule` evaluates the first lambda and, if it returns `true`, evaluates the second. The composition is built once and reused for every input; there is no per-call allocation or re-construction.

This pattern scales. A rule may be the conjunction of three, four, or a dozen sub-rules, and the resulting composite is still a single `Predicate` value. The sub-rules can be named individually, which makes the rule readable and individually testable, while the composite is the value that production code actually applies.

## A Numeric Range Validator

A common use of predicate composition is range validation. A value is acceptable when it lies within an inclusive lower and upper bound. Expressed directly, the rule is the conjunction of a lower-bound predicate and an upper-bound predicate.

```cpp
auto lo = dsl::predicate([](int x) { return x >= 0; });
auto hi = dsl::predicate([](int x) { return x <= 100; });
auto in_range = lo & hi;

in_range(50);   // true
in_range(-1);   // false, short-circuits at lo
in_range(101);  // false, lo passes, hi fails
```

Expressed this way, the rule mirrors the way a specification would describe it: "the value must be greater than or equal to zero, and less than or equal to one hundred." Each bound is a named, independently testable predicate. The composite is the conjunction of the two.

Because conjunction short-circuits, a value below the lower bound never reaches the upper-bound check. For a pair of integer comparisons this is a trivial saving, but the same structure applies to predicates whose evaluation is expensive — a database lookup, a regular-expression match, a remote service call. Ordering the operands so that the cheaper predicate is evaluated first is a simple and effective optimisation.

## A String Validator

String validators benefit from composition because real-world string rules are rarely single-condition. A username might need to be non-empty, begin with a letter, and contain only alphanumeric characters. Each of these is a separate predicate, and the composite is their conjunction.

```cpp
#include "DSLtk.hpp"
#include <cctype>
#include <string>
#include <string_view>

auto non_empty = dsl::predicate([](std::string_view s) {
  return !s.empty();
});
auto starts_alpha = dsl::predicate([](std::string_view s) {
  return !s.empty() && std::isalpha(static_cast<unsigned char>(s[0]));
});
auto alnum_only = dsl::predicate([](std::string_view s) {
  for (char c : s)
    if (!std::isalnum(static_cast<unsigned char>(c))) return false;
  return true;
});

auto valid_username = non_empty & starts_alpha & alnum_only;
```

The composite `valid_username` is a single predicate that accepts only strings satisfying all three sub-rules. Because `operator&` is left-associative, the expression is parsed as `(non_empty & starts_alpha) & alnum_only`, which is exactly the intended grouping. Short-circuiting means that an empty string is rejected by the first operand and never inspected by the later, more expensive checks.

This example also illustrates that the argument type does not have to be `int`. The call operator is a template on the argument type, so any type the underlying callable accepts can be passed. A predicate declared against `std::string_view` accepts both `std::string` and `std::string_view` arguments, and even string literals, through implicit conversion.

## Disjunction for Alternative Shapes

Disjunction is the natural tool when a value is acceptable if it matches any one of several shapes. Consider a configuration value that may be either a small integer constant or one of a fixed set of sentinel strings. The two cases are most readable as separate predicates joined by `|`.

```cpp
auto small_int = dsl::predicate([](int x) { return x >= 0 && x < 8; });
auto sentinel  = dsl::predicate([](std::string_view s) {
  return s == "auto" || s == "default" || s == "inherit";
});
```

Mixing argument types across the operands of a single composite is only meaningful if the composite is applied to values that all operands can accept. In practice, a configuration system that holds `std::variant<int, std::string>` values would write a small adapter predicate that dispatches on the variant's index, and the disjunction would then be expressed inside that adapter. The Operators feature does not impose any type unification; it simply forwards the argument to each underlying callable and relies on overloading or generic lambdas to handle the dispatch.

A more straightforward disjunction involves a single value type. A port-number validator might accept the privileged range, the registered range, or the dynamic range, with any of the three being acceptable.

```cpp
auto privileged = dsl::predicate([](int p) { return p >= 0 && p <= 1023; });
auto registered = dsl::predicate([](int p) { return p >= 1024 && p <= 49151; });
auto dynamic    = dsl::predicate([](int p) { return p >= 49152 && p <= 65535; });
auto valid_port = privileged | registered | dynamic;
```

Each branch is named and independently meaningful. The composite is a single predicate that accepts any valid TCP or UDP port number. Short-circuiting ensures that once a branch returns `true`, the remaining branches are not evaluated.

## Negation in Practice

Negation is most often used to invert a predicate whose positive form is the natural way to describe a condition. A rule that rejects values equal to a sentinel can be written as the negation of an equality check.

```cpp
auto is_sentinel = dsl::predicate([](int x) { return x == -1; });
auto not_sentinel = !is_sentinel;
auto usable = not_sentinel & in_range;   // in_range defined above
```

The composite `usable` accepts values in range that are not the sentinel. Writing the rule this way keeps the specification aligned with the prose: "the value must not be the sentinel, and must be in range." The alternative — writing a single lambda that combines both conditions — would obscure the structure of the rule and would not allow the sub-rules to be tested in isolation.

Negation also composes with disjunction. De Morgan's laws apply directly: `!(a & b)` is equivalent to `!a | !b`, and `!(a | b)` is equivalent to `!a & !b`. The library does not perform any such rewriting; it evaluates the expression as written. The choice of which form to use is a matter of readability, not of efficiency, because both forms produce the same observable short-circuit behaviour up to the order of evaluation.

## Composing Three or More Predicates

Because each operator returns a fresh `Predicate`, there is no fixed arity on composition. Three predicates can be conjoined with two uses of `&`, four with three, and so on. The resulting value is a single `Predicate` whose closure contains copies of every underlying callable, nested through the closure types of the intermediate composites.

```cpp
auto positive = dsl::predicate([](int x) { return x > 0; });
auto even     = dsl::predicate([](int x) { return x % 2 == 0; });
auto small    = dsl::predicate([](int x) { return x < 100; });
auto good     = positive & even & small;
```

The expression `positive & even & small` is parsed as `(positive & even) & small` because `operator&` is left-associative. The inner `positive & even` produces a `Predicate` whose closure holds copies of the first two lambdas; the outer `&` then produces a `Predicate` whose closure holds a copy of that intermediate and a copy of the third lambda. Evaluation walks the tree in left-to-right order, short-circuiting at the first `false`.

The same structure applies to disjunction and to mixed compositions. The depth of the resulting closure tree is proportional to the number of operands, but in practice this is irrelevant: the tree is flattened by the compiler's inliner, and a chain of conjunctions compiles down to a sequence of calls with branches. There is no runtime cost to building the composite, and the per-call cost is the cost of the underlying callables plus a small number of branches.

## Mixed Conjunction, Disjunction, and Negation

Real rules often combine all three operators. A password-policy rule, for example, might require that a candidate be at least twelve characters long, contain at least one digit, and not be one of a small set of well-known weak passwords.

```cpp
auto long_enough = dsl::predicate([](std::string_view s) {
  return s.size() >= 12;
});
auto has_digit = dsl::predicate([](std::string_view s) {
  for (char c : s) if (std::isdigit(static_cast<unsigned char>(c))) return true;
  return false;
});
auto is_common = dsl::predicate([](std::string_view s) {
  return s == "password123" || s == "letmein12345";
});
auto accept = long_enough & has_digit & !is_common;
```

The composite reads almost like the specification. Each sub-rule is named, the conjunctions short-circuit, and the negation inverts the common-password check. A reviewer reading this code can verify each sub-rule against the policy document without untangling a single large lambda.

Mixed compositions also arise in numeric validation. A value might need to be positive and either even or divisible by five. Expressed directly, this is a conjunction whose right-hand operand is a disjunction.

```cpp
auto divisible_by_5 = dsl::predicate([](int x) { return x % 5 == 0; });
auto rule = positive & (even | divisible_by_5);
```

The parentheses are load-bearing here, and their significance is the topic of the next section.

## Operator Precedence and Parentheses

C++ assigns `!` higher precedence than `&`, and `&` higher precedence than `|`. These precedences are inherited by the overloaded operators because overload resolution does not change the parsing of expressions. An expression written without parentheses is parsed according to the language rules, which may not match the intended grouping.

The expression `a & b | c` is parsed as `(a & b) | c`, not as `a & (b | c)`. If the intended rule is "a holds, and either b or c holds," the parentheses are mandatory. Omitting them silently produces a different rule that accepts any value for which `c` holds, regardless of `a`.

The expression `!a & b` is parsed as `(!a) & b`, which is usually what the writer intends. The expression `!a | b` is parsed as `(!a) | b`, again usually intended. Even in these cases, explicit parentheses are recommended because they make the grouping visible to a reader who may not have the precedence table memorised.

A simple discipline eliminates the entire class of precedence bugs: whenever two different operators appear in the same expression, parenthesise every binary operand. Unary `!` applied to a single named predicate does not require parentheses, but `!(a & b)` does, because the operand of `!` is a binary expression. The library cannot enforce this discipline; it is a coding-standard matter.

```cpp
// Intended: positive AND (even OR divisible by 5).
auto rule = positive & (even | divisible_by_5);

// Different rule, parsed as (positive & even) | divisible_by_5.
auto wrong = positive & even | divisible_by_5;
```

The second line above is not a bug in the library; it is the standard C++ parsing of an unparenthesised expression. The Operators feature overloads operators that already have precedences, and those precedences are honoured by the compiler. The recommendation throughout this manual is to parenthesise.

## Value Semantics and Capture

Every composition produces a fresh `Predicate` that captures its operands by copy. The composite has no reference back to the operands, no shared mutable state, and no allocation. Once the expression has been evaluated, the operands can go out of scope and the composite continues to behave correctly.

This is a direct consequence of the way the operators are defined. Each operator's lambda initialises two captures, `a = fn` and `b = other.fn`, by copy. The captures are members of the closure type, and the closure is in turn the member `fn` of the new `Predicate`. The composite is a value that owns its constituents.

Value semantics have two practical consequences. First, a composite predicate can be returned from a function, stored in a container, or moved across threads without any lifetime concerns. The composite is self-contained. Second, the operands are not modified by composition. A predicate can be used in multiple composites, and each composite holds an independent copy of the underlying callable.

For stateful callables — functors with non-`const` members, lambdas that capture by reference — the value-semantics model still holds at the level of the `Predicate`, but the user must reason about the lifetimes of the captured references. A lambda that captures a local variable by reference and is wrapped in a `Predicate` produces a predicate that dangles once the local goes out of scope. This is the same rule that applies to any capturing lambda; the Operators feature neither introduces nor eliminates the hazard.

## Constexpr Evaluation

Every operation in the feature is `constexpr`: the constructor, the call operator, all three composition operators, and the `predicate` factory. A predicate can be constructed, composed, and evaluated entirely at compile time. This makes the feature suitable for use in `static_assert`, in template arguments, and in any other context that requires a constant expression.

```cpp
constexpr auto is_positive = dsl::predicate([](int x) { return x > 0; });
constexpr auto is_even     = dsl::predicate([](int x) { return x % 2 == 0; });
constexpr auto rule        = is_positive & is_even;

static_assert(rule(4));
static_assert(!rule(-2));
static_assert(!rule(3));
```

The lambdas in this example are themselves constexpr, which is permitted since C++17. The `Predicate` constructor and the `operator&` body are constexpr, so the composite `rule` is a constant expression. The `static_assert` lines evaluate the composite at compile time and fail the build if the rule does not hold.

Compile-time evaluation is useful for encoding invariants that must hold for a program to be correct. A configuration table whose entries must satisfy a complex rule can be validated at compile time, so that a violation is a build error rather than a runtime failure. Chapter 18 discusses memoization, which can be combined with compile-time predicates to build lookup tables; Chapter 19 discusses lazy values, which can defer the evaluation of a predicate until its result is actually needed.

## An Even-and-Positive Rule

A frequent teaching example is the rule "the value is positive and even." The Operators feature expresses this rule directly from its prose description.

```cpp
auto is_positive = dsl::predicate([](int x) { return x > 0; });
auto is_even     = dsl::predicate([](int x) { return x % 2 == 0; });
auto rule        = is_positive & is_even;

rule(4);   // true
rule(3);   // false, odd
rule(-2);  // false, negative
```

This is the example used in the header's own documentation. It is deliberately small because the point is to show the structure of the feature, not to solve a realistic problem. The same structure scales to the larger validators shown earlier in this chapter.

The short-circuit behaviour is visible in the second and third calls. For input `3`, `is_positive` returns `true` and `is_even` returns `false`; the composite returns `false`. For input `-2`, `is_positive` returns `false` and `is_even` is not evaluated; the composite returns `false`. The observable result is the same in both cases, but the path through the rule differs.

## Composing User-Defined Predicates

The factory accepts any callable, including user-defined functors. A functor with named members can be more readable than a lambda when the rule is non-trivial, and it can be reused across several composites.

```cpp
struct in_closed_range {
  int lo, hi;
  constexpr bool operator()(int x) const { return x >= lo && x <= hi; }
};

auto adult_age   = dsl::predicate(in_closed_range{18, 120});
auto senior_age  = dsl::predicate(in_closed_range{65, 120});
auto either      = adult_age | senior_age;   // equivalent to adult_age here
auto neither     = !either;
```

A functor with state — here, the bounds `lo` and `hi` — is captured by value into the `Predicate`, and the composite captures the `Predicate` by value in turn. The bounds are part of the rule, not part of the value being tested, so they belong in the functor. This pattern is appropriate when a single functor type can represent a family of related rules parameterised by a few integers.

Functors are also useful when a rule requires helper methods or carries documentation. A lambda is anonymous and cannot carry comments attached to its type, while a named struct can be documented as a unit. For a rule that appears in a public API, the named struct is often preferable.

## A Password-Policy Rule

Combining the techniques above, a realistic password policy can be expressed as a single composite predicate. The policy requires a minimum length, an upper-case letter, a lower-case letter, a digit, and the absence of a common weak password.

```cpp
auto long_enough = dsl::predicate([](std::string_view s) {
  return s.size() >= 12;
});
auto has_upper = dsl::predicate([](std::string_view s) {
  for (char c : s) if (std::isupper(static_cast<unsigned char>(c))) return true;
  return false;
});
auto has_lower = dsl::predicate([](std::string_view s) {
  for (char c : s) if (std::islower(static_cast<unsigned char>(c))) return true;
  return false;
});
auto has_digit = dsl::predicate([](std::string_view s) {
  for (char c : s) if (std::isdigit(static_cast<unsigned char>(c))) return true;
  return false;
});
auto is_common = dsl::predicate([](std::string_view s) {
  return s == "Password123!" || s == "Letmein12345";
});

auto policy = long_enough & has_upper & has_lower & has_digit & !is_common;
```

The composite is a single value that can be stored in a variable, passed to a validation function, or applied to many candidates. Each sub-rule is independently testable, which simplifies unit testing: a test for `has_digit` can pass strings with and without digits without setting up the rest of the policy.

The order of the operands is significant for performance but not for correctness. The cheapest checks — length and the presence of a digit — appear first, so that an expensive check like `is_common` is only reached when the cheap checks have already passed. For a string that is too short, none of the character-class scans is performed. This kind of ordering is a minor optimisation, but it is free: the conjunction short-circuits, so reordering the operands costs nothing and may save work.

## Short-Circuit Semantics in Detail

The short-circuit behaviour of the composite predicates is a direct consequence of using the built-in `&&` and `||` operators inside the lambdas. The built-in operators guarantee that the right-hand operand is not evaluated when the left-hand operand settles the result. Because the lambdas invoke the captured callables as ordinary function calls, the same guarantee applies to the predicates.

For conjunction, the right-hand callable is not invoked when the left-hand callable returns `false`. For disjunction, the right-hand callable is not invoked when the left-hand callable returns `true`. Negation always invokes its single operand.

This behaviour matters in three situations. First, when the right-hand predicate has side effects — logging, instrumentation, mutation of a captured counter — those side effects are skipped when the left-hand predicate settles the result. Second, when the right-hand predicate is expensive — a regular-expression match, a database lookup — the cost is avoided for inputs that the left-hand predicate already rejects. Third, when the right-hand predicate has preconditions that the left-hand predicate guarantees — for example, indexing into a container that the left-hand predicate has checked is non-empty — the short-circuit ensures that the precondition is met before the right-hand predicate runs.

The library does not document or enforce any of these uses. It simply inherits the short-circuit semantics of the built-in operators, and the user may rely on them as they would rely on `&&` and `||` in ordinary C++ code.

## The Difference from Bitwise Operators

The `&`, `|`, and `!` tokens are overloaded for `Predicate` to mean Boolean conjunction, disjunction, and negation. In ordinary C++ for integral operands, `&` and `|` are bitwise operators that evaluate both operands and combine their bit patterns. The overload set for `Predicate` does not change the meaning of these operators for other types; it only provides additional overloads that are selected when both operands of `&` or `|` are `Predicate` values, and when the operand of `!` is a `Predicate`.

A common mistake is to combine a `Predicate` with a raw callable using `&`. The expression `pred & lambda` does not compile, because the right-hand operand is not a `Predicate` and the overload is not selected. The raw callable must first be wrapped with `dsl::predicate` or `make_pred`. The `explicit` constructor prevents an implicit conversion from the callable to a `Predicate`, which would otherwise make the expression compile but with surprising semantics.

Another common mistake is to combine a `Predicate` with a `bool` using `&`. The expression `pred(x) & true` performs a bitwise AND of two `bool` values, which is well-defined but evaluates both operands. This is rarely the intent; the user almost always means `pred(x) && true`, which uses the built-in logical AND. The library cannot prevent this mistake because `pred(x)` is a `bool`, not a `Predicate`, and the overloaded operators do not apply.

## Capturing State in Lambdas

A lambda that captures by value produces a closure with value members, and the `Predicate` holds that closure by value. This is the safe default: the composite is self-contained and has no lifetime concerns. A lambda that captures by reference produces a closure with reference members, and the `Predicate` holds that closure by value — but the references inside the closure still refer to the original objects.

A predicate that captures a reference to a local variable and is returned from the function in which it was created produces a dangling reference. This is the same hazard that applies to any capturing lambda, and the Operators feature neither introduces nor eliminates it. The user must ensure that any object referred to by a captured reference outlives the predicate.

For mutable state, a lambda declared `mutable` produces a closure whose members can be modified by the call operator. Wrapping such a lambda in a `Predicate` produces a predicate whose `operator()` is `const`, which means the call operator cannot modify the closure. The library's `operator()` is `const`-qualified, so a mutable lambda's modifications are not observable through a `Predicate`. This is deliberate: a predicate is an immutable rule, and side effects that mutate the rule's own state would break value semantics. Users who need mutable state should hold the state outside the predicate and capture it by reference, with the lifetime considerations that entails.

## A Compile-Time Table Validator

Combining `constexpr` evaluation with a small data structure, a program can validate a table of constants at compile time. The build fails if any entry violates the rule, which is a strong guarantee: the program cannot be compiled with an invalid table.

```cpp
constexpr bool check_table() {
  auto positive = dsl::predicate([](int x) { return x > 0; });
  auto even     = dsl::predicate([](int x) { return x % 2 == 0; });
  auto rule     = positive & even;

  int table[] = {2, 4, 6, 8, 10};
  for (int x : table)
    if (!rule(x)) return false;
  return true;
}

static_assert(check_table());
```

This pattern is useful for embedded tables, lookup constants, and any data that is known at compile time and must satisfy an invariant. The predicate is constructed inside the `constexpr` function, applied to each entry, and the result is checked by `static_assert`. A violation is a hard build error, which is the strongest form of validation available in C++.

The same pattern can be used with `consteval` to force evaluation at compile time even when the result is not used in a constant-expression context. A `consteval` function that constructs and applies a predicate is guaranteed to run entirely during compilation, with no runtime overhead.

## Predicates in Pipelines

Chapter 6 introduced the Pipeline feature, which threads values through a chain of transforms. Predicates compose naturally with pipelines: a value can be transformed and then tested, or a predicate can be used as a gate that decides whether a transform is applied.

The two features are independent. A DSL may list both in its feature list, as the basic-usage example shows, and the factories `pipe` and `make_pred` coexist without interference. A predicate is not a pipe, and a pipe is not a predicate; the two are different kinds of values that can be used together in the same program.

A typical combined use applies a pipeline to produce a value and then applies a predicate to test the result. The predicate is constructed once, outside the pipeline, and applied to the pipeline's output. This separation keeps the transformation logic and the validation logic distinct, which simplifies both.

## Predicates and the Rewrite Feature

The Rewrite feature, covered in Chapter 13, uses predicates to decide whether a rewrite rule applies to a given AST node. The predicates used by Rewrite are ordinary callables; wrapping them in `dsl::Predicate` is not required by the Rewrite machinery, but it is permitted. A wrapped predicate can be composed with `&`, `|`, and `!` before being handed to a rewrite rule, which allows the rule's applicability condition to be expressed as a combination of simpler checks.

This use is mentioned here only to point forward; the details of Rewrite are the subject of Chapter 13 and Chapter 14. The relevant point for this chapter is that the Operators feature produces ordinary function objects, which can be consumed by any API that expects a callable taking a node and returning a `bool`.

## Performance Considerations

The Operators feature is designed to add no runtime overhead relative to hand-written composition. Each operator constructs a `Predicate` whose closure holds copies of the operands; the closure is inlined by the compiler, and the resulting code is equivalent to a sequence of calls with branches. There is no heap allocation, no virtual dispatch, and no type erasure.

The call operator of the composite is a single call to the stored lambda, which in turn calls the two captured callables and combines their results with `&&` or `||`. The compiler inlines the lambda body, inlines the captured callables, and produces a sequence of tests with short-circuit branches. For simple callables — integer comparisons, character scans — the generated code is indistinguishable from a hand-written conditional.

The cost of constructing a composite is proportional to the number of operands, because each operand is copied into the closure. For lambdas with small captures, this cost is negligible. For functors with large captures, the cost may be significant, and the composite should be constructed once and reused rather than reconstructed for each input. The value-semantics model encourages this pattern: a composite is a value that can be stored in a variable and applied many times.

## Common Pitfalls

The most common pitfall is operator precedence. As discussed earlier, an unparenthesised expression mixing `&` and `|` is parsed according to C++ rules, which may not match the intended grouping. The remedy is to parenthesise every binary operand whenever two different operators appear in the same expression.

A second pitfall is the difference between the overloaded operators and the built-in bitwise operators. Combining a `Predicate` with a non-`Predicate` using `&` or `|` does not select the overload and may produce a bitwise operation or a compile error. The remedy is to wrap every callable in `dsl::predicate` before combining it.

A third pitfall is capturing by reference in lambdas that outlive the captured objects. A predicate that captures a local by reference and is returned from the enclosing function produces a dangling reference. The remedy is to capture by value, or to ensure that the captured object outlives the predicate.

A fourth pitfall is the `explicit` constructor. The expression `pred & [](...){...}` does not compile because the lambda is not implicitly convertible to a `Predicate`. The remedy is to use the `dsl::predicate` factory, which performs the explicit construction. The `explicit` qualifier is deliberate, because implicit construction from arbitrary callables would make the overload set surprising.

A fifth pitfall is assuming that `!` distributes over a composite. The library does not rewrite `!(a & b)` into `!a | !b`; it evaluates the expression as written, which means the inner conjunction is evaluated first and then negated. The two forms are observationally equivalent up to short-circuit order, but they are not the same value. A user who wants the De Morgan form must write it explicitly.

## A Complete Example

The following complete program exercises the feature end to end. It defines a DSL with the Operators feature, constructs several predicates, composes them into a rule, and applies the rule to a series of inputs.

```cpp
#include "DSLtk.hpp"
#include <cctype>
#include <iostream>
#include <string_view>

struct App : dsl::DSL<App, dsl::Operators> {};

int main() {
  using App::make_pred;

  auto positive = make_pred([](int x) { return x > 0; });
  auto even     = make_pred([](int x) { return x % 2 == 0; });
  auto small    = make_pred([](int x) { return x < 100; });

  auto good_int = positive & even & small;

  for (int x : {-2, 0, 3, 4, 50, 102, 64}) {
    std::cout << x << ": " << good_int(x) << "\n";
  }

  auto non_empty   = make_pred([](std::string_view s) { return !s.empty(); });
  auto starts_lt   = make_pred([](std::string_view s) {
    return !s.empty() && s[0] == 'L';
  });
  auto tag         = non_empty & starts_lt;

  for (std::string_view s : {"", "Hello", "Lib", "lab"}) {
    std::cout << s << ": " << tag(s) << "\n";
  }
}
```

The program defines two rules. The first accepts positive, even integers below one hundred. The second accepts non-empty strings that begin with the letter `L`. Each rule is a single value, applied to a series of inputs. The output of the program is the Boolean result of each test, printed one per line.

## Summary

The Operators feature adds three overloaded operators to a small wrapper type, `dsl::Predicate<F>`, turning any callable into a composable Boolean rule. Conjunction is `operator&`, disjunction is `operator|`, and negation is `operator!`. Each operator returns a fresh `Predicate` that captures its operands by value, so a composite is a self-contained value with no shared mutable state and no allocation. Short-circuit semantics are inherited from the built-in `&&` and `||` operators, so a failing left-hand operand skips the right-hand operand's evaluation.

The `dsl::predicate` factory deduces the callable type using `std::decay_t`, so lambdas and functors can be wrapped without spelling out their closure types. The `dsl::Operators` feature tag exposes the same factory through a DSL-qualified `make_pred` static member function, following the same mixin protocol used by the other features described in Chapter 5. Both factories are `constexpr`, and every operation in the feature is `constexpr`, so rules can be constructed and evaluated at compile time for use in `static_assert` and other constant-expression contexts.

The chief practical advice is to parenthesise every binary operand whenever two different operators appear in the same expression, because the overloaded operators inherit the C++ precedences and an unparenthesised expression may not group as intended. The other pitfalls — mixing `Predicate` with raw callables, capturing by reference in long-lived predicates, and the `explicit` constructor — all reduce to the same principle: the feature provides a small, strict wrapper, and the user is responsible for handing it wrapped values and sensible lifetimes. With those caveats observed, the feature allows complex validation rules to be expressed as readable expressions of named sub-rules, with no runtime overhead relative to hand-written conditionals.
