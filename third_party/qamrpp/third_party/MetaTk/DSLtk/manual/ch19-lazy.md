# Chapter 19: `Lazy<T>`: Deferred Single-Shot Values

## Motivation

Many computations inside an embedded DSL are expensive to perform and not always needed. A configuration parser may compute a derived default that the user ultimately overrides. A tree rewriting pass (Chapter 13) may carry a candidate replacement that is only consumed when a fixpoint (Chapter 14) actually converges. An expression-template evaluation (Chapters 15 and 16) may produce an intermediate value that the surrounding pipeline later discards. In each case, paying for the computation eagerly is wasted work.

`dsl::Lazy<T>` is the toolkit's mechanism for this situation. It wraps a producer callable and defers its invocation until the value is first read. Once read, the result is cached and reused for every subsequent read. The producer therefore runs at most once over the lifetime of a given `Lazy<T>` instance — the single-shot guarantee that gives the chapter its name.

This is a different shape of deferral from expression templates. `BinExpr`, `UnaryExpr`, and the fused evaluation machinery of Chapter 16 defer a *tree* of operations so that the tree can be traversed, rewritten, or fused before any node fires. `Lazy<T>` defers one thunk. It is the smallest possible unit of lazy evaluation: a value that is not there yet but will be, exactly when needed, exactly once.

The type lives in the `dsl` namespace alongside `Memoization` (Chapter 18) and the monadic helpers (Chapter 21). Together with `Maybe<T>` (Chapter 20) and `Result<T,E>` (Chapter 22), it forms the small value-level utility layer that supports the larger DSL machinery. This chapter describes its contract, its semantics, and the principal patterns of use.

## The `Lazy<T>` Interface

The complete public interface of `Lazy<T>` is small enough to fit on one screen. The class is parameterized on the produced value type `T`. It exposes a single constructor accepting a `std::function<T()>`, three accessor/query members, and no other surface. The header overview summarizes it plainly: "Defers one computation until first get()/force() and caches it."

```cpp
namespace dsl {
template <typename T> class Lazy
{
public:
  explicit Lazy (std::function<T ()> producer);

  T & get ();        // computes on first access, caches, returns reference
  T & force ();      // alias of get()
  bool is_computed () const noexcept;

private:
  std::function<T ()> producer_;
  std::optional<T> value_{};
};
} // namespace dsl
```

The constructor is `explicit`, which prevents implicit conversions from arbitrary `std::function<T()>` objects and forces the caller to name the type at the construction site. This is consistent with the design pillars introduced in Chapter 1: the toolkit prefers named, intention-revealing construction over clever implicit glue.

The two state members are private. `producer_` holds the deferred callable, and `value_` is a `std::optional<T>` that begins disengaged. The optional doubles as the "has it been forced?" flag: when `value_` is engaged, the producer has already run and its result is cached. There is no separate boolean; the optional's engagement state *is* the forced state.

## Construction

A `Lazy<T>` is built by handing a zero-argument callable to the constructor. The callable must return something convertible to `T`. Because the constructor takes a `std::function<T()>`, lambdas, function pointers, and arbitrary function objects all work.

```cpp
dsl::Lazy<int> answer([]{ return 42; });

int (*fp)() = []{ return 7; };
dsl::Lazy<int> seven(fp);

struct Producer {
  int operator()() const { return 99; }
};
dsl::Lazy<int> ninety_nine{Producer{}};
```

The producer is stored by value inside the `std::function`. Lambdas that capture by value therefore carry their captured state into the lazy object; lambdas that capture by reference carry references, and the usual lifetime caveats apply. The producer is never copied again after construction unless the `Lazy<T>` itself is copied.

The header overview gives the canonical form, which matches the example fragment in the doc comment:

```cpp
dsl::Lazy<int> v([]{ return 42; });
```

The chapter's running examples use this style: a single lambda, capturing whatever it needs, returning the deferred value.

## First-Access Semantics

The defining behavior of `Lazy<T>` is that the producer is invoked on first access and never again. This is the single-shot property. The `get()` member implements it directly, and the verbatim definition from the header is:

```cpp
T &
get ()
{
  if (!value_)
    value_ = producer_ ();
  return *value_;
}
```

Three things happen here, in order. The member checks whether `value_` is engaged. If it is not, it invokes `producer_()` and assigns the result, engaging the optional. Finally it dereferences the optional and returns a reference to the stored `T`. The branch is taken at most once across the lifetime of a given instance; once `value_` is engaged, the `if` body is skipped forever.

Because `get()` returns `T&`, callers may mutate the cached value after the fact. This is intentional: the cache is the value, not a const snapshot. A caller that wants immutability should hold a `const` reference or copy the value out.

The returned reference is stable for the lifetime of the `Lazy<T>`. It does not dangle unless the `Lazy<T>` itself is destroyed. This makes `get()` suitable for forming pointers and references that downstream code will keep.

## `force()` and `is_computed()`

`force()` is a verbatim alias of `get()`:

```cpp
T &
force ()
{
  return get ();
}
```

The two names exist for readability at the call site. `force()` reads as an imperative command — "compute this now" — and is appropriate when the caller is deliberately materializing a value whose computation is being scheduled. `get()` reads as ordinary access and is appropriate when the caller simply wants the value and is indifferent to whether computation happens now or has already happened.

`is_computed()` is a non-mutating query that reports whether the producer has fired:

```cpp
bool
is_computed () const noexcept
{
  return value_.has_value ();
}
```

It is `const` and `noexcept`, so it can be called on a `const Lazy<T>` and never throws. It is useful for diagnostics, for asserting that a value has been pre-materialized before a hot path, and for building layers that inspect the deferred state without forcing it.

Note that `is_computed()` is *not* a way to peek at the value without forcing. It reports only whether forcing has already happened. There is no `try_get()` or "peek" accessor in the interface; the design is deliberately minimal.

## Single-Shot Guarantees

The single-shot property has several consequences worth stating precisely. First, the producer runs at most once, regardless of how many times `get()` or `force()` is called. Second, the producer runs at most once even if the lazy is copied after being forced — the copy carries the engaged cache, not a fresh producer invocation. Third, the producer runs at most once even if `get()` is called concurrently, in the sense that the implementation does not promise any synchronization; see the section on thread safety below.

The guarantee is structural, not magical. It follows from the fact that `value_` is engaged exactly when the producer has run, and `get()` only invokes the producer when `value_` is disengaged. There is no path that re-disengages `value_`, so there is no path that re-invokes the producer.

This makes `Lazy<T>` a natural fit for producers with side effects that must fire exactly once: a logging message that announces initialization, a file handle that must be opened once, a registration that mutates global state. Because the producer fires at most once, the side effect fires at most once.

## Value Semantics

`Lazy<T>` is a regular value type. The implicitly defined copy constructor copies both `producer_` and `value_`. Copying an un-forced lazy yields a fresh un-forced lazy whose producer is a copy of the original; the producer in the copy has not yet run. Copying a forced lazy yields a forced lazy whose cache is a copy of the original's cached value; the producer in the copy will never run, because the cache is already engaged.

```cpp
dsl::Lazy<int> a([]{ return 5; });
dsl::Lazy<int> b = a;        // b is un-forced; b.producer_ is a copy of a.producer_
b.get();                     // fires b's producer, caches 5
dsl::Lazy<int> c = b;        // c is forced; c.value_ == 5; c.producer_ never runs
```

Move construction transfers ownership of the internal state. A moved-from `Lazy<T>` is left in a valid but unspecified state, as is usual for standard-library types holding `std::function` and `std::optional` members. In practice both members will be in their moved-from states: an empty `std::function` and a disengaged `std::optional`.

Copy and move assignment behave correspondingly. Because the type holds only standard-library value types, the implicit definitions are correct and no special member functions are declared.

The practical consequence is that `Lazy<T>` can be stored in containers, returned from functions, and passed by value without surprises. The deferred nature is preserved across copies of an un-forced lazy, and the cached nature is preserved across copies of a forced one.

## A Lazy Constant

The simplest non-trivial use is a lazy constant: a value whose computation is deferred but whose result does not depend on any later state. The header's own example is of this form.

```cpp
#include <DSLtk.hpp>
#include <string>

int main() {
  dsl::Lazy<std::string> cfg([]{ return std::string("ready"); });
  auto s = cfg.get();
  std::printf("%s\n", s.c_str());   // ready
}
```

The producer returns a `std::string` literal. The first call to `get()` invokes the producer, stores the string in the optional cache, and returns a reference to it. A second call to `get()` returns the same reference without re-running the lambda.

For a constant whose computation is genuinely cheap, `Lazy<T>` adds only overhead. Its value appears when the constant is expensive to compute — for example, a cryptographic digest, a parsed default configuration, or a numerical table that must be built by iteration.

## A Lazy Config Value

A more realistic example is a configuration value whose default is derived from environment or filesystem state that may not be available at construction time. The producer captures the parameters it needs and performs the lookup only when the value is read.

```cpp
dsl::Lazy<std::string> config_path([]{
  if (const char *env = std::getenv("APP_CONFIG"))
    return std::string(env);
  return std::string("/etc/app/default.conf");
});

// ... potentially never read if the user passes --config on the command line ...

std::ifstream in(config_path.get());
```

If the command-line parser ultimately decides that the default path is not needed, the producer never runs and the `getenv` call is never made. If the default path is needed, the producer runs exactly once and the result is reused for any subsequent read.

This pattern composes well with the option-handling idioms of Chapter 7. A predicate composed with `Operators` may gate access to the lazy value; the lazy value itself is independent of the predicate machinery and is simply read when the predicate succeeds.

## A Lazy-Loaded Resource

Resource handles are a natural fit. Opening a file, establishing a network connection, or allocating a GPU buffer are all expensive operations that should be deferred until the resource is actually used. Wrapping the acquisition in a `Lazy<T>` defers it without obscuring the type of the handle.

```cpp
dsl::Lazy<std::shared_ptr<Database>> db([]{
  auto handle = std::make_shared<Database>("dsn://prod");
  handle->connect();
  return handle;
});

void on_request() {
  // The database is connected on the first request, not at program start.
  db.get()->query("SELECT 1");
}
```

The single-shot guarantee ensures that the connection is opened exactly once, even if `on_request()` is called many times. The returned `std::shared_ptr<Database>&` is stable, so callers may hold raw pointers or references to the underlying database.

Because `get()` returns a mutable reference, callers may also reconfigure the resource after acquisition — for example, by setting a connection timeout on the cached handle. The mutation persists in the cache and is visible to subsequent readers.

## Composing Lazy Values

`Lazy<T>` composes by ordinary function application: one lazy may depend on another by reading it inside its producer. Because each producer fires at most once, the dependency chain materializes in dependency order and caches each intermediate result.

```cpp
dsl::Lazy<int> raw([]{ return load_raw_value(); });
dsl::Lazy<int> normalized([&]{
  int r = raw.get();                 // forces raw, caches it
  return (r - offset) * scale;
});

int v = normalized.get();            // forces normalized, which forces raw
int w = raw.get();                   // returns cached raw, no re-load
```

The producer of `normalized` captures `raw` by reference and calls `raw.get()` inside its body. When `normalized.get()` is first called, `normalized`'s producer runs, which in turn calls `raw.get()`, which forces `raw`. Both caches are now engaged, and both producers are retired.

This composition is the lazy analog of function composition. Where `f(g(x))` composes two eager functions, `Lazy<R>{[&]{ return f(g.get()); }}` composes two deferred computations. The shape is identical; only the timing differs.

A longer chain works the same way. The only constraint is that the dependency graph must be acyclic; a cycle would recurse without terminating, because no producer in the cycle would ever find its dependency already forced.

## Breaking Initialization-Order Cycles

A common use of lazy values in larger systems is to break initialization-order cycles between modules that reference each other. Two modules that each need a handle to the other at construction time can instead hold lazy references that materialize on first use, by which point both modules exist.

```cpp
struct Scheduler;
struct WorkerPool;

dsl::Lazy<WorkerPool*> pool;     // filled in later
dsl::Lazy<Scheduler*>  sched;    // filled in later

struct WorkerPool {
  WorkerPool() { /* do not touch sched here */ }
  void run() {
    Scheduler *s = sched.get();  // safe: scheduler exists by now
    s->enqueue(this);
  }
};
```

The lazy references defer the cross-module dereference until after both modules have been constructed. This is the same technique used in dependency-injection frameworks under the name "lazy proxy"; `Lazy<T>` provides it in a single header without a framework.

Within DSLtk itself, this pattern is relevant to the pipeline feature of Chapter 6 and the task pipelines of Chapter 26, where stages may reference each other's outputs. A lazy reference lets a stage hold a handle to a downstream result without requiring the downstream stage to have already executed.

## Contrast with Memoization

Chapter 18's `Memoization` caches the results of a pure function by argument. `dsl::memoize<R, A>(f)` returns a callable that remembers the result of `f(a)` for each `a` it has seen. The cache is keyed on the argument and grows as new arguments appear.

`Lazy<T>` is the special case where there is no argument. It caches exactly one value, the result of a zero-argument producer. Equivalently, `Lazy<T>` is to `Memoization` as a thunk is to a memoized function: the thunk has no parameters, so there is only one result to remember.

```cpp
// Memoization: many results, keyed by argument.
auto square = dsl::memoize<int, int>([](int x){ return x * x; });
square(3);   // computes 9, caches under 3
square(4);   // computes 16, caches under 4
square(3);   // returns cached 9

// Lazy: one result, no argument.
dsl::Lazy<int> single([]{ return expensive_constant(); });
single.get();   // computes once, caches
single.get();   // returns cached
```

The two utilities are complementary. When a computation depends on an argument, use `Memoization`. When it does not, use `Lazy<T>`. A `Lazy<T>` can in fact be implemented as a `Memoization<R, Unit>` with a unit argument, but the dedicated type is clearer at the call site and avoids carrying a meaningless key.

## Contrast with Expression Templates

Expression templates, covered in Chapters 15 and 16, defer a *structure* of computations. A `BinExpr<L, Op, R>` object represents an unevaluated binary operation; the operation is performed only when the expression is evaluated, and the structure may be rewritten or fused before evaluation. The deferral is structural: the tree is the value.

`Lazy<T>` defers a single leaf. There is no structure to rewrite, no fusion to perform, and no tree to traverse. The producer is an opaque thunk; the lazy value is either forced or not, and once forced it is just a `T`.

The distinction matters when choosing which tool to reach for. If the computation has shape that downstream passes may want to inspect or transform — for example, to apply the rewrite rules of Chapter 13 — then an expression template is the right vehicle. If the computation is an opaque black box whose only relevant property is "do it once, later," then `Lazy<T>` is the right vehicle.

The two can be combined. A `Lazy<BinExpr<...>>` defers the construction of an expression tree until it is first read, at which point the tree is built and cached. Subsequent reads return the cached tree, which can then be evaluated or rewritten as needed. This is a useful pattern when the tree is expensive to build but cheap to evaluate.

## A Lazy Constant From a Function

The producer need not be a lambda; any callable yielding `T` works. A free function that returns a computed constant is a common choice, especially when the constant is shared across translation units and the producer's logic deserves a name.

```cpp
long long compute_hash_table_seed() {
  long long seed = 0xcbf29ce484222325LL;
  for (int i = 0; i < 256; ++i)
    seed = (seed ^ i) * 0x100000001b3LL;
  return seed;
}

dsl::Lazy<long long> seed(compute_hash_table_seed);

long long lookup(int key) {
  // The seed is computed on first lookup, cached thereafter.
  return hash(key, seed.get());
}
```

Naming the producer as a free function has two benefits. It makes the computation greppable and unit-testable independent of the lazy wrapper, and it keeps the construction site short. The `Lazy<T>` constructor accepts the function pointer directly because `std::function<T()>` is constructible from it.

## Lazy Values in Data Members

A common placement of `Lazy<T>` is as a class data member. The member is initialized in the constructor's member-initializer list with a lambda that captures `this`, deferring some computation that depends on the fully-constructed object.

```cpp
struct Session {
  std::string token;
  dsl::Lazy<std::string> signature;

  Session(std::string t)
    : token(std::move(t))
    , signature([this]{ return sign(token); })
  {}

  void send() {
    transmit(token, signature.get());   // sign() runs here, once
  }
};
```

The lambda captures `this` by value, which is safe because the lambda is only ever invoked through the `Lazy<T>` member, which lives as long as the `Session` itself. The producer never outlives the object whose state it reads.

This idiom is valuable when a derived value is needed only by some methods of the class. Eager computation in the constructor would penalize every construction, including those that never call the method that needs the derived value. Lazy computation in a member penalizes only the constructions whose objects actually use the value.

Capturing `this` in a member lazy requires care if the object is movable. After a move, the `this` captured by the producer refers to the moved-from object, not the new one. To avoid this hazard, either disable moves for the owning class, force the lazy in the move constructor, or capture state by value rather than by pointer.

## Forcing Early

Sometimes a value should be materialized before the first natural read — for example, to move the cost out of a latency-sensitive path, or to surface producer errors during initialization rather than during a request. `force()` exists for exactly this purpose.

```cpp
dsl::Lazy<Config> cfg([]{ return load_config(); });

void init() {
  cfg.force();          // materialize now; errors surface here
  // ... rest of init ...
}

void handle_request() {
  cfg.get();            // cheap: returns cached reference
}
```

Calling `force()` is equivalent to calling `get()` and discarding the result. The producer fires if it has not already fired, and the cache is engaged for all future reads. After `force()`, `is_computed()` returns `true`.

Because `force()` is just `get()`, it is safe to call repeatedly. The second and later calls are no-ops that return the cached reference. There is no "unforce" operation; once engaged, the cache cannot be cleared except by destroying the `Lazy<T>`.

## Producers with Side Effects

Because the producer runs at most once, side effects inside the producer fire at most once. This is both the principal benefit and the principal hazard of `Lazy<T>`.

The benefit is that one-shot side effects — opening a file, registering a callback, logging an initialization message — can be tied to the lazy value without risk of duplication. The producer is the natural place for the acquisition half of a RAII pattern whose release is handled elsewhere.

The hazard is that the timing of the side effect is coupled to the timing of the first read. A producer that logs "config loaded" will log that message at whatever point the config is first accessed, which may be deep inside a request handler rather than during startup. If the log message is meant to announce startup, the producer should be forced explicitly during initialization.

A producer that mutates external state is similarly coupled. Mutations fire on first read, not on construction. If the mutation must happen before some other initialization step, the lazy must be forced before that step.

Producers should therefore be *pure-ish*: they may have side effects, but those side effects should be idempotent or at least insensitive to exactly when they fire. A producer that must fire at a specific time should be forced explicitly at that time.

## The Result Type Must Be `T`

The template parameter `T` is the type that the producer must yield. The constructor takes `std::function<T()>`, so any callable whose return type is convertible to `T` will be accepted, and the conversion happens inside the `std::function` at invocation time.

```cpp
dsl::Lazy<double> pi([]{ return 3.14159265358979; });   // double, exact match
dsl::Lazy<double> half([]{ return 1; });                 // int -> double, converted
```

The cached value is stored as `T`, not as the producer's raw return type. This means conversions are applied exactly once, on the first read, and the cached value has the declared type `T`. Subsequent reads return a `T&` with no further conversion.

The choice of `T` therefore matters. A `Lazy<int>` whose producer returns `double` will truncate the double on the first read and cache the truncated int. A `Lazy<std::string>` whose producer returns `const char*` will construct a `std::string` on the first read and cache it. The conversion is not free; it is part of the cost paid on the first access.

`T` must be a type that is usable inside `std::optional<T>`. This means `T` must be copyable or movable, destructible, and not a reference or an array. In practice every value type in the toolkit satisfies these requirements.

## Thread Safety

The header overview states the policy directly: "This type is not thread-safe by default." The `get()` member performs a check-then-set sequence on `value_` without any synchronization. Concurrent calls to `get()` on the same `Lazy<T>` from multiple threads may race, with outcomes ranging from double invocation of the producer to torn reads of the optional's internal state.

The single-threaded design is deliberate. Adding internal synchronization would impose a cost on every access, defeating the purpose of a lightweight thunk. Callers that need cross-thread sharing are expected to provide their own synchronization at a coarser granularity, or to force the lazy on a single thread before publishing the cached value to other threads.

A common pattern is to force the lazy during single-threaded startup and then publish the resulting reference to worker threads that only read. Because the cache is engaged after forcing, worker threads see a stable value and the producer never fires again.

```cpp
dsl::Lazy<Table> table([]{ return build_table(); });

void start() {
  table.force();   // single-threaded startup
  spawn_workers([&](auto &t){ t.run(table.get()); });
}
```

This pattern is safe because the producer fires exactly once on the startup thread, before any worker thread reads the cache.

## Lifetime of Captured State

When a producer lambda captures by reference, the references must remain valid for as long as the lazy may be forced. If the lazy is forced before the captured objects go out of scope, all is well. If the lazy outlives the captured objects, forcing it dereferences dangling references.

```cpp
dsl::Lazy<int> bad() {
  int local = 7;
  return dsl::Lazy<int>([&]{ return local; });   // dangling: local leaves scope
}
```

This is the ordinary rule for by-reference captures in returned lambdas, and `Lazy<T>` does nothing to soften it. The safe approaches are the ordinary ones: capture by value, capture a pointer to a longer-lived object, or force the lazy before returning.

When capturing by value, the captured state is stored inside the lambda, which is stored inside the `std::function`, which is stored inside the `Lazy<T>`. The captured state therefore lives as long as the lazy itself. This is the safest style and the one recommended for producers that may outlive their lexical scope.

## A Worked Example: A Lazy Parser Table

The chapter's final example ties several threads together. It builds a parser table lazily, composes it with a lazy default value, and forces it explicitly at a controlled point. The shape is representative of how `Lazy<T>` appears in the parser-combinator chapters (Chapters 23 through 25).

```cpp
#include <DSLtk.hpp>
#include <string>
#include <unordered_map>

struct RuleTable {
  std::unordered_map<std::string, int> ids;
};

RuleTable build_table() {
  RuleTable t;
  t.ids["expr"]   = 0;
  t.ids["term"]   = 1;
  t.ids["factor"] = 2;
  return t;
}

int main() {
  dsl::Lazy<RuleTable> table(build_table);
  dsl::Lazy<std::string> default_rule([&]{
    return table.get().ids.contains("expr") ? "expr" : "epsilon";
  });

  // Startup: force both, surface any errors here.
  table.force();
  default_rule.force();

  // Hot path: pure cache reads, no producer invocations.
  const std::string &r = default_rule.get();
  std::printf("default rule: %s\n", r.c_str());
}
```

The table is built once and cached. The default rule is computed once from the cached table and cached in turn. The hot path reads both caches by reference and performs no allocations. If the table-building or default-rule logic ever changes, only the producers need editing; the call sites are unchanged.

## Pitfalls

The principal pitfalls are straightforward consequences of the design. The producer fires once, so side effects fire once; if the timing of a side effect matters, force the lazy at the right time. The producer is stored in a `std::function`, so lambdas with large captures pay the `std::function` overhead — usually acceptable, occasionally not. The cache is mutable, so callers that hold a `T&` may be surprised if another caller mutates it. The type is not thread-safe, so cross-thread sharing requires external synchronization or pre-forcing.

A subtler pitfall is relying on `is_computed()` to mean "the value is available without cost." `is_computed()` reports only whether the producer has *already* run; it does not peek at the value. A caller that wants the value must call `get()`, which will fire the producer if it has not fired. There is no way to read the cached value conditionally without forcing it; if that capability is needed, the caller must track the forced state externally.

Another subtle pitfall is moving a `Lazy<T>` whose producer captures `this`. The captured `this` is not adjusted by the move, so the producer in the moved-to object still refers to the moved-from object. This is the same hazard as any `this`-capturing member lambda, and the remedy is the same: force before moving, or capture state by value.

Finally, the type is header-only and relies on the implicitly defined special member functions. This means `Lazy<T>` is copyable whenever `T` and `std::function<T()>` are copyable, which is essentially always. Callers that need to forbid copying must wrap the type rather than modify it.

## Summary

`dsl::Lazy<T>` is a deferred single-shot value. It wraps a `std::function<T()>` producer and a `std::optional<T>` cache, exposing `get()` and `force()` to materialize the value on first access and `is_computed()` to query whether materialization has already happened. The producer runs at most once; subsequent reads return a stable `T&` into the cache.

The type is a regular value type: copyable, movable, storable in containers, returnable from functions. Copying an un-forced lazy yields a fresh un-forced lazy; copying a forced lazy yields a forced lazy whose cache is a copy of the original's value. The implicitly defined special member functions are correct because the members are standard-library value types.

`Lazy<T>` is the right tool when a computation has no argument and should be performed at most once. It complements `Memoization` (Chapter 18), which caches by argument, and differs from expression templates (Chapters 15 and 16), which defer a structure rather than a single thunk. It is not thread-safe by design, and producers with side effects should be pure-ish or forced at a controlled time. Together with `Maybe<T>` (Chapter 20), the monadic helpers (Chapter 21), and `Result<T,E>` (Chapter 22), it forms the small value-level utility layer that supports the larger DSL machinery introduced in Chapter 1.
