# Chapter 18: Memoization: `memoize` and `MemoizedCallable`

## Motivation

Many embedded-DSL computations re-evaluate the same pure function on the same inputs. A recursive descent parser may re-derive the same sub-parse for the same cursor position; a rewrite cost model may re-query the same node signature; a numeric DSL may recompute the same closed-form expression for the same coefficient tuple. Each redundant call wastes work that the program has, by definition, already performed once. Memoization is the standard remedy: store the result of a pure function keyed by its argument, and on subsequent calls with the same key return the stored result without recomputation.

DSLtk provides a small, deliberate facility for this. The `dsl::MemoizedCallable<Key, Value, Fn>` class template wraps a callable together with an `std::unordered_map` cache, and the `dsl::memoize<Key, Value>(fn)` factory returns an instance of that wrapper. The result is a value-semantic, single-threaded, header-only memoization helper that integrates cleanly with the rest of the toolkit. This chapter documents the facility in full: its API, its cache key contract, its value semantics, and the situations in which it should and should not be used.

The design is intentionally narrow. There is no eviction policy, no locking, no expiration, and no instrumentation. The cache is a plain member of a plain object, and the user owns its lifetime and its growth. This austerity is consistent with the design pillars introduced in Chapter 1: the library depends on no threading library, allocates only through the standard containers the user already pays for, and exposes composable value types rather than singletons or global state.

## What Memoization Is

Memoization is a transformation applied to a function. Given a function `f` of one argument, memoization produces a new function `g` such that `g(x)` returns `f(x)` on the first call and returns the previously computed `f(x)` on every subsequent call with the same `x`. The transformation is correct only when `f` is pure: deterministic, side-effect-free, and independent of any mutable external state. Under that assumption the cached value is provably equal to the value that a fresh call would produce, so returning it changes nothing observable.

The cache is keyed by the argument. A hashable, equality-comparable key type is therefore the only structural requirement. The value type may be anything copyable or movable enough to be stored in an `unordered_map`. When the key type is the function's natural argument type, the wrapper is transparent to callers: they invoke `g(x)` exactly as they would have invoked `f(x)`.

The benefit is asymptotic. A naive recursive Fibonacci runs in exponential time; a memoized Fibonacci runs in linear time, because each distinct argument is computed once and then served from the cache. The same shape applies to any dynamic-programming recurrence, to top-down parser combinators that re-explore the same position, and to any cost function whose recursion overlaps itself.

## The `Memoization` Feature Tag

DSLtk separates the memoization *callable* from the `Memoization` *feature tag*. The tag, declared in the feature machinery of Chapter 5, is a mixin intended for derived DSLs that wish to expose a `clear_cache()` member driven by the DSL object's own `memo_cache` member. It is the hook by which a DSL built on `dsl::DSL` (Chapter 3) can offer cache invalidation as a first-class operation on the DSL object itself.

```cpp
struct Memoization
{
  template <typename Derived> struct Mixin
  {
    void clear_cache ()
    {
      static_cast<Derived &> (*this).memo_cache.clear ();
    }
  };
};
```

The mixin assumes that the derived DSL exposes a member named `memo_cache` that itself exposes `.clear()`. The library does not constrain that member with a concept; the contract is structural and is satisfied by any standard unordered associative container. A DSL that mixes in `Memoization` and declares a suitable `memo_cache` member gains a uniform `clear_cache()` entry point, which is useful when a long-lived DSL object accumulates entries across many independent inputs and must be reset between runs.

This tag is orthogonal to the `MemoizedCallable` wrapper. A program may use `dsl::memoize` without ever instantiating a `dsl::DSL` derivative, and a DSL may mix in `Memoization` without using `MemoizedCallable`. The two facilities address related but distinct needs: the wrapper memoizes a single free callable, while the tag gives a DSL object a uniform cache-clear hook for whatever cache the DSL chooses to maintain internally.

## The `MemoizedCallable` Class Template

The wrapper itself is a class template with three parameters: the cache key type, the cached value type, and the wrapped callable type. The key and value types are explicit because they cannot in general be deduced from a lambda's signature, and because forcing the user to name them documents the cache contract at the call site.

```cpp
template <typename Key, typename Value, typename Fn>
class MemoizedCallable
{
public:
  explicit MemoizedCallable (Fn fn) : fn_ (std::move (fn)) {}

  Value operator() (const Key &key)
  {
    if (auto it = cache_.find (key); it != cache_.end ())
      return it->second;
    auto value = fn_ (key);
    cache_.emplace (key, value);
    return value;
  }

  void clear_cache () { cache_.clear (); }

private:
  Fn fn_;
  std::unordered_map<Key, Value> cache_{};
};
```

The constructor is explicit, which prevents implicit conversions from a raw callable into a `MemoizedCallable`. The intent is that construction goes through the `dsl::memoize` factory, which is the idiomatic entry point; the explicit constructor exists primarily so that the factory can name the type. Users who prefer to construct directly may do so, at the cost of spelling out all three template arguments.

The cache is an `std::unordered_map<Key, Value>` value-initialized in the member initializer list. The default constructor of `unordered_map` yields an empty map with the default bucket count and the default hasher and key-equal predicate. The user therefore supplies a `std::hash<Key>` specialization implicitly when one exists for the standard library, and supplies a custom hasher only by replacing the map type, which the wrapper does not currently expose as a parameter. In practice this means the key type must be hashable by `std::hash` out of the box.

The callable is stored by value. A lambda captured into `MemoizedCallable` is copied or moved into `fn_`, and the wrapper owns the sole copy. This matters for lambdas that capture by value: each captured value lives once, inside the wrapper, and is consulted on every cache miss. Lambdas that capture by reference are accepted by the type system but are dangerous, for reasons discussed later in this chapter.

## The `operator()` Definition

The heart of the wrapper is its call operator. It is worth quoting verbatim, because every semantic claim in this chapter follows from its four lines.

```cpp
Value operator() (const Key &key)
{
  if (auto it = cache_.find (key); it != cache_.end ())
    return it->second;
  auto value = fn_ (key);
  cache_.emplace (key, value);
  return value;
}
```

The operator takes a single argument of type `const Key&`. This is a deliberate restriction: the wrapper memoizes a unary function. Multi-argument memoization is supported only by choosing a key type that bundles several values together, such as a `std::tuple` or a user-defined aggregate, and by providing a hash for that key. The single-argument form keeps the cache lookup and the function call syntactically identical, which is the property that makes the wrapper a drop-in replacement for the original callable.

The lookup uses `cache_.find(key)`. This requires that `Key` be equality-comparable and that the map's hasher accept a `Key`. A hit returns `it->second` by value, which copies the stored result out of the map. A miss invokes `fn_(key)`, stores the result with `cache_.emplace(key, value)`, and then returns the local copy. The value is therefore computed exactly once per distinct key, and every subsequent call with that key returns the cached copy.

Note that the operator is non-`const`. It mutates `cache_` on a miss, so it cannot be invoked on a `const MemoizedCallable`. A program that needs to share a memoized callable across const-aware interfaces must store it behind indirection and expose a non-const access path. This is consistent with the value-semantic, owning design: the cache is part of the object's state, and consulting it is a mutating operation.

## The `memoize` Factory

Constructing a `MemoizedCallable` directly requires naming `Fn`, which is usually an unspellable lambda closure type. The `dsl::memoize` factory removes that burden through template argument deduction.

```cpp
template <typename Key, typename Value, typename Fn>
auto memoize (Fn &&fn)
{
  return MemoizedCallable<Key, Value, std::decay_t<Fn>>{ std::forward<Fn> (fn) };
}
```

The user specifies `Key` and `Value` explicitly and passes the callable; the factory deduces `Fn`, decays it to remove references and cv-qualifiers, and constructs the wrapper. The use of `std::forward<Fn>(fn)` preserves the value category of the argument: an lvalue callable is copied into the wrapper, and an rvalue callable (such as a temporary lambda) is moved.

The return type is `MemoizedCallable<Key, Value, std::decay_t<Fn>>`, which, because the factory returns `auto`, is deduced at the call site. The caller typically binds the result to an `auto` variable, which is the recommended style. Naming the wrapper type explicitly is possible but verbose, and offers no benefit in ordinary use.

```cpp
auto square = dsl::memoize<int, int>([](int x){ return x * x; });
assert (square (7) == 49);
```

The factory is the only supported construction path. It is a single function template with no overloads, no specializations, and no customization points. This narrowness is intentional: the memoization facility is small enough to fit in a page of source, and its behavior is fully determined by the three template parameters and the four lines of the call operator.

## Cache Key Requirements

The key type `Key` participates in two operations: hashing and equality. The wrapper uses `std::unordered_map<Key, Value>` with its default template arguments, so the hasher is `std::hash<Key>` and the equality predicate is `std::equal_to<Key>`. A type is a valid key when both are usable.

For the standard integer and pointer types, `std::hash` specializations are provided by the standard library. The same is true for `std::string`, `std::string_view`, and most smart pointers. Memoizing a function whose argument is one of these types requires no additional work from the user. The cache simply works, because the standard library has already supplied the necessary hasher.

For user-defined key types, the situation is more involved. A `struct` with no `std::hash` specialization will fail to instantiate the `unordered_map` and will produce a compile-time diagnostic. The user must either specialize `std::hash` for the key type or accept that the type cannot be used as a key. The library does not provide a generic tuple hasher, which has a concrete consequence for multi-argument memoization.

```cpp
struct Point2 { int x, y; };
bool operator==(const Point2&, const Point2&) = default;

namespace std {
template <> struct hash<Point2> {
  size_t operator()(const Point2& p) const noexcept {
    return hash<int>{}(p.x) ^ (hash<int>{}(p.y) << 1);
  }
};
}
```

With that hasher in place, a `Point2` may be used as a key directly. Without it, the same code fails to compile. The burden of providing a hasher is the user's, and it is the single most common reason that a memoization attempt fails to build.

## The Single-Argument Contract

The call operator accepts exactly one argument. This is a hard structural constraint, not a stylistic recommendation. A callable that takes two parameters cannot be memoized by direct substitution; it must be adapted either by binding one argument or by accepting a tuple key.

The simplest adaptation, when one argument is fixed for the lifetime of the cache, is to capture it in the lambda. The wrapped callable then takes the remaining argument, which becomes the key. This is appropriate when the fixed argument is a configuration value, a reference to an immutable table, or a DSL object whose identity does not change between calls.

```cpp
auto cost = dsl::memoize<int, double>(
  [weights](int id){ return weights[id] * 1.5; });
```

When both arguments vary independently, the only faithful adaptation is to pack them into a single key. The natural pack is `std::tuple<A, B>`, but `std::hash<std::tuple<A, B>>` is not provided by the standard library. The user must supply a hasher, typically by xoring the hashes of the elements, as shown for `Point2` above. Once supplied, the tuple becomes an ordinary key.

```cpp
auto md = dsl::memoize<std::tuple<int,int>, long long>(
  [](std::tuple<int,int> k){ return 1LL; });
```

This trade-off is intentional. A multi-argument memoization facility built into the library would either have to impose a hashing strategy on the user or expose a hasher template parameter, both of which would complicate the API. The single-argument form keeps the wrapper minimal and leaves the key design to the caller.

## Value Semantics and Copying

`MemoizedCallable` is a value type. It owns its callable and its cache by value, and its copy constructor and copy assignment operator are the implicitly generated defaults. Copying a `MemoizedCallable` copies the callable and the entire `unordered_map`, including every cached entry.

This is the behavior most callers expect, but it has a cost. Copying a wrapper with a large cache is an O(n) operation that allocates and re-hashes. Passing a memoized callable by value into a function therefore duplicates the cache, which is rarely desired. The recommended practice is to pass memoized callables by `const` reference, or to wrap them in a `std::shared_ptr` when shared ownership of a single cache is required across multiple call sites.

```cpp
long long driver (const dsl::MemoizedCallable<int, long long,
                    decltype([](int){ return 0LL; })>& fib, int n);
```

The explicit type in the signature above is unwieldy, which is why the `auto` return type of `dsl::memoize` is usually bound to a local variable and passed by reference. When a memoized callable must cross an API boundary, the common idiom is to define a named function object type for the callable and to name the `MemoizedCallable` instantiation explicitly, or to type-erase behind `std::function`.

Move semantics are also the implicitly generated defaults. Moving a `MemoizedCallable` moves both the callable and the map, leaving the source in a valid but unspecified state. A moved-from wrapper may be assigned to or destroyed, but its cache contents are not guaranteed. Returning a `MemoizedCallable` from a function therefore costs nothing beyond the moves of its two members, both of which are cheap for moved-from standard containers.

## Single-Threaded by Design

The wrapper performs no locking. The cache is a plain member, and the call operator reads and writes it without synchronization. Two concurrent calls to the same `MemoizedCallable` race on the `unordered_map`, which is undefined behavior. The library does not link against, or depend on the presence of, any threading library; this is one of the design pillars rehearsed in Chapter 1.

The consequence is that a memoized callable is suitable for use within a single thread, or for use across threads only when the caller serializes access externally. A mutex held around each call would make the wrapper safe but would also serialize every lookup, eliminating much of the benefit of memoization for parallel workloads. Programs that need concurrent memoization typically memoize per-thread, with one wrapper instance per worker, so that each thread's cache is private and uncontended.

The `clear_cache()` member is likewise unsynchronized. Clearing a cache while another thread is consulting it is a data race. The intended usage is to call `clear_cache()` from the same thread that performs lookups, or to establish a happens-before relationship through some external mechanism before clearing from another thread.

This austerity is a feature. The wrapper is a few lines of code with no hidden machinery, no lock contention, no allocator hooks, and no surprises. Where concurrency is needed, it is the caller's responsibility to add it in the shape that fits the surrounding program, rather than the library's responsibility to guess at a one-size-fits-all strategy.

## A Memoized Fibonacci

The canonical demonstration of memoization is the Fibonacci sequence. A naive recursive implementation recomputes the same values exponentially many times; a memoized implementation computes each value once.

```cpp
#include <DSLtk.hpp>
#include <iostream>

int main ()
{
  auto fib = dsl::memoize<int, long long>(
    [](int n) -> long long {
      if (n < 2) return n;
      return 1LL; // placeholder; see below
    });

  std::cout << fib(10) << "\n";
}
```

The placeholder above reveals a subtlety. The lambda passed to `dsl::memoize` cannot refer to the `MemoizedCallable` that will eventually wrap it, because that object does not exist at the point of capture. A recursive memoized function therefore requires the callable to consult the cache by routing its recursive calls through the wrapper.

The standard idiom is to capture a pointer or a reference to the wrapper, which is only possible if the wrapper is declared before the lambda is constructed. Because the lambda is passed to the factory that constructs the wrapper, the pointer must be set after construction. A two-step initialization, with the lambda capturing a pointer by reference to a variable declared outside, achieves this.

```cpp
dsl::MemoizedCallable<int, long long,
  std::function<long long(int)>>* p = nullptr;

auto fib = dsl::memoize<int, long long>(
  [&p](int n) -> long long {
    if (n < 2) return n;
    return (*p)(n - 1) + (*p)(n - 2);
  });
p = &fib;

std::cout << fib(45) << "\n";
```

Here the lambda captures `p` by reference, and `p` is assigned the address of `fib` immediately after construction. The recursive calls `(*p)(n-1)` and `(*p)(n-2)` go through the wrapper, so each distinct argument is computed once. The cache grows to at most `n+1` entries, and the total work is linear rather than exponential.

This idiom is slightly more ceremony than a self-referential `std::function` would require, but it avoids the type erasure and allocation that `std::function` imposes. The wrapper's `Fn` parameter remains the lambda's closure type, so calls through `fn_` are direct invocations of the closure's `operator()`, with no virtual dispatch or small-object-buffer copy.

## A Memoized Ackermann

The Ackermann function is a second classic. It grows faster than any primitive recursive function, and its recursion is deeply overlapping, which makes it a sharp test of any memoization scheme.

```cpp
dsl::MemoizedCallable<std::pair<int,int>, long long,
  std::function<long long(std::pair<int,int>)>>* ack_p = nullptr;

auto ack = dsl::memoize<std::pair<int,int>, long long>(
  [&ack_p](std::pair<int,int> k) -> long long {
    auto [m, n] = k;
    if (m == 0) return n + 1;
    if (n == 0) return (*ack_p)({m - 1, 1});
    return (*ack_p)({m - 1, (*ack_p)({m, n - 1})});
  });
ack_p = &ack;

std::cout << ack({2, 3}) << "\n"; // 9
std::cout << ack({3, 4}) << "\n"; // 125
```

The key here is `std::pair<int,int>`, which unlike `std::tuple` does have a `std::hash` specialization in many implementations. Where the specialization is absent, the user must supply one. The pair bundles the two arguments into a single key, satisfying the wrapper's unary contract while preserving the two-argument recursion.

Ackermann is also a cautionary example: the cache grows without bound, and for large inputs it will grow until the process exhausts memory. The wrapper imposes no limit, performs no eviction, and offers no way to inspect the cache size except through the underlying `unordered_map`, which is private. The user who memoizes a function with an unbounded argument domain must be aware that the cache is a leak by design.

## A Memoized String Normalizer

Memoization is not limited to numeric recursion. Any pure transformation on a hashable input is a candidate. A string normalizer that canonicalizes whitespace, lowercases letters, and strips diacritics is a good example: the same inputs recur in real corpora, and the transformation is expensive enough that caching pays.

```cpp
auto normalize = dsl::memoize<std::string, std::string>(
  [](std::string s) -> std::string {
    std::string out;
    out.reserve(s.size());
    bool last_space = false;
    for (char c : s) {
      char lc = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
      if (lc == ' ') { if (!last_space) out.push_back(' '); last_space = true; }
      else { out.push_back(lc); last_space = false; }
    }
    return out;
  });

assert (normalize("  Hello   World ") == " hello world ");
```

The key is `std::string`, which the standard library hashes. The value is also `std::string`. Each distinct input string is normalized once; subsequent calls with the same string return the cached result by copy. The cache keys and values are owned by the map, so long-lived inputs keep their strings alive inside the cache for the lifetime of the wrapper.

The cost trade-off here is between the normalization work and the string allocation. For very short inputs, the allocation may dominate the work, and memoization may be a net loss. For inputs of moderate length, where normalization involves a scan and a copy, the cache usually wins on the second call. As always, the decision to memoize is a performance hypothesis that should be tested against the actual workload.

## A Memoized Recursive Cost Function

DSLtk itself is an embedded-DSL toolkit, and a realistic internal use of memoization is a recursive cost function over an AST. Suppose each node has a cost that depends on the costs of its children, and suppose the tree contains shared subtrees. A naive recursive walk recomputes the cost of each shared subtree as many times as it appears; a memoized walk computes each subtree's cost once.

```cpp
auto node_cost = dsl::memoize<const Node*, double>(
  [&node_cost](const Node* n) -> double {
    if (!n) return 0.0;
    double c = n->local_cost;
    for (const Node* ch : n->children)
      c += node_cost(ch);
    return c;
  });
```

The pointer `node_cost` is captured by reference, which is the recursive idiom from the Fibonacci example. The key is `const Node*`, which the standard library hashes as a pointer. Equality on pointers is identity, so two calls with the same node address hit the cache, while two calls with distinct but equal-valued nodes miss. This is the correct behavior when the AST uses pointer identity for sharing, as most AST implementations do.

The cache here is bounded by the number of distinct nodes in the tree, which is finite and usually small. This is the benign case for memoization: a bounded domain, an expensive function, and overlapping calls. The wrapper pays for itself on the first traversal of any tree with shared subtrees, and continues to pay for itself on subsequent traversals of the same tree if the wrapper is kept alive.

The same pattern applies to parser combinators (Chapter 23), where a memoized parse at a given cursor position prevents re-derivation of the same sub-parse, and to rewrite cost models (Chapter 14), where a memoized signature prevents re-evaluation of the same rewrite sequence. In each case the key is a position or a signature, the value is a parse or a cost, and the cache turns an exponential search into a polynomial one.

## When to Use Memoization

Memoization is appropriate when three conditions hold simultaneously. The function must be pure, so that cached values remain valid. The function must be expensive enough, or called often enough, that the cost of the cache lookup is less than the cost of recomputation. And the argument domain must be bounded, or at least bounded in practice, so that the cache does not grow without limit.

The first condition is the most important and the most frequently violated. A function that reads from a global variable, that depends on the current time, or that mutates state as a side effect is not pure, and memoizing it produces silently wrong results. The wrapper cannot detect impurity; it caches whatever the function returns on the first call, and returns that value forever after. The user is the sole judge of purity, and the user bears the consequences of a wrong judgment.

The second condition is a performance question. A function that adds two integers is cheaper to compute than to look up in a hash map, and memoizing it makes the program slower. A function that parses a string, solves a differential equation, or searches a large state space is usually cheaper to look up than to compute, and memoizing it makes the program faster. The crossover point depends on the function and on the hash map's load factor, and is best found by measurement rather than by intuition.

The third condition is a memory question. A function whose argument domain is small and fixed, like a chess move generator keyed by board position, has a naturally bounded cache. A function whose argument domain is large or unbounded, like a file-path normalizer keyed by arbitrary strings, has a cache that grows with the input stream. In the bounded case the cache is a permanent asset; in the unbounded case the cache is a leak that must be managed, either by clearing it periodically or by accepting that the process will eventually run out of memory.

## When Not to Use Memoization

The mirror image of the above is a set of clear contraindications. A function that returns different results for the same argument on different calls is not memoizable. A random number generator, a clock-reading function, and a counter that increments on each call all fall into this category. Memoizing them caches the first result and returns it forever, which is almost never what the caller wants.

A function whose result depends on external state is equally unsuitable, even if it does not mutate that state. A function that reads a configuration file, queries a database, or fetches a network resource returns a value that may change between calls, and a cache that fixes the first value will hide subsequent updates. The cache is correct only if the external state is immutable for the lifetime of the wrapper, which is a strong assumption that should be made explicit, not silent.

Arguments that do not hash are a structural contraindication. A function that takes a stream, a file handle, or a non-hashable struct as its argument cannot be memoized without first adapting the argument to a hashable key. Sometimes the adaptation is natural, as when a file descriptor is replaced by a path string; sometimes it is lossy, as when a stream is replaced by its current position, which may not uniquely determine the stream's state. The user must judge whether the adaptation preserves the purity contract.

Finally, memoization is inappropriate when the cache would grow without bound and the program cannot tolerate that growth. A long-running server that memoizes a function keyed by client input will accumulate cache entries for as long as it runs, and will eventually exhaust memory. The wrapper provides no eviction, no LRU, no TTL, and no size limit. If any of these is required, the user must implement them outside the wrapper, or choose a different caching strategy.

## Pitfalls

The most common pitfall is capturing mutable state in the wrapped lambda. If the lambda captures a variable by reference and that variable changes between calls, the function is no longer pure, and the cache returns stale results. The type system permits the capture, and the wrapper cannot detect the mutation, so the bug manifests as silently wrong output that appears only on the second and later calls.

```cpp
int factor = 2;
auto scaled = dsl::memoize<int, int>(
  [&factor](int x){ return x * factor; });
assert (scaled(5) == 10);
factor = 3;
assert (scaled(5) == 10); // still 10, not 15
```

The second assertion holds because the cache was populated when `factor` was 2. The mutation of `factor` is invisible to the wrapper, which returns the cached 10 rather than recomputing with the new factor. The fix is either to capture `factor` by value, freezing it at construction, or to call `clear_cache()` after mutating the captured state, explicitly invalidating the stale entries.

A second pitfall is using a large argument type as a key. The cache stores the key by value in the `unordered_map`, and hashing a large key on every lookup is expensive. A key that is a thousand-character string, a large struct, or a deep container will dominate the cost of the lookup, and may make the cache slower than recomputation. The remedy is to key on a digest of the argument, such as a hash of the string, accepting the small probability of a collision in exchange for a dramatic reduction in key cost.

A third pitfall is relying on cache ordering. The `unordered_map` does not preserve insertion order, and the order of iteration over the cache is unspecified. A program that depends on the order in which cached entries are visited, or that assumes that the most recently inserted entry is the last one iterated, will behave unpredictably. The cache is a lookup structure, not a sequence, and should be treated as such.

A fourth pitfall is thread-safety assumptions. The wrapper is single-threaded, and a program that shares a memoized callable across threads without external synchronization will race on the cache. The symptoms of such a race are nondeterministic and may include duplicated computation, lost updates, and, in the worst case, corruption of the `unordered_map`'s internal structure. The remedy is per-thread caches or external locking, as discussed earlier.

## Cache Lifetime and Clearing

The cache lives as long as the `MemoizedCallable` that owns it. When the wrapper is destroyed, the cache is destroyed with it, and every cached value is freed. There is no global cache, no shared state, and no need to explicitly release entries; the cache's lifetime is exactly the wrapper's lifetime.

The `clear_cache()` member provides a way to empty the cache without destroying the wrapper. It is useful when the wrapper is long-lived but the inputs come in batches, and the cache from one batch is not relevant to the next. Clearing between batches bounds the cache size to the size of the largest batch, rather than the cumulative size of all batches, which is often the difference between a bounded cache and an unbounded one.

```cpp
auto lookup = dsl::memoize<std::string, Record>(fetch_record);
for (auto& batch : batches) {
  for (const auto& id : batch) process(lookup(id));
  lookup.clear_cache();
}
```

The `clear_cache()` member is also exposed by the `Memoization` feature tag's mixin, which forwards to `Derived::memo_cache.clear()`. A DSL that mixes in the tag and maintains its own cache can therefore offer the same `clear_cache()` interface on the DSL object itself, giving the user a uniform way to reset memoized state regardless of whether the cache lives in a `MemoizedCallable` or in the DSL.

There is no way to remove a single entry from the cache through the public API. The wrapper exposes only `operator()` and `clear_cache()`; the underlying `unordered_map` is private. A user who needs per-entry eviction must either accept the all-or-nothing `clear_cache()` or implement a custom wrapper. This is a deliberate simplification: per-entry eviction raises questions of policy (LRU, LFU, TTL) that the library declines to answer, and the all-or-nothing interface keeps the wrapper small and its behavior obvious.

## Relationship to Lazy Evaluation

Memoization and lazy evaluation are related but distinct. A lazy value (Chapter 19) is a deferred computation that is evaluated at most once, on first demand. A memoized callable is an eager computation that is evaluated once per distinct argument, on every demand. The lazy value has a single key (the absence of an argument) and a single cached value; the memoized callable has a key space and a cache of one entry per key.

The two compose naturally. A memoized callable whose value type is a `dsl::Lazy<T>` returns a lazy value on each distinct key, and the lazy value itself is evaluated at most once. This composition is useful when the function's result is expensive to compute but is not always fully consumed by the caller; the memoization ensures the function is called once per key, and the laziness ensures the result is computed once per call, even if the caller only inspects part of it.

The conceptual boundary is sharper than it first appears. A `dsl::Lazy<T>` is a single thunk with a single cached result; it has no key space, and it is not callable with arguments. A `MemoizedCallable<K, V, F>` is callable with a key, and it maintains a map from keys to results. Confusing the two leads to designs that try to memoize a thunk with arguments or to lazy-evaluate a function without a cache, both of which are better expressed by the other abstraction.

## Integration with DSLtk Concepts

The memoization facility interacts with the rest of the toolkit through the feature machinery of Chapter 5. The `Memoization` feature tag is a mixin that a DSL derived from `dsl::DSL` may include in its feature list, alongside `LazyFeature`, `Monadic`, and the other tags. When included, it adds the `clear_cache()` member to the DSL object, subject to the DSL also declaring a `memo_cache` member.

The concepts that govern feature tags, documented in Chapter 5, apply to `Memoization` as they apply to every other tag. The tag is an empty struct with a nested `Mixin` template; the mixin is injected into the derived DSL's base class list by the CRTP machinery of Chapter 3. The user who designs a DSL with memoization support includes the tag in the feature list and declares a `memo_cache` member of a suitable container type, and the mixin provides the rest.

The `MemoizedCallable` wrapper, by contrast, is independent of the feature machinery. It does not require a `dsl::DSL` derivative, does not participate in CRTP, and does not depend on any concept. It is a free-standing value type that may be used in any C++20 program that includes the header. This independence is deliberate: memoization is useful enough that confining it to DSL-derived types would be an arbitrary restriction, and the wrapper's value semantics make it composable with any ownership strategy the caller chooses.

## Summary

`dsl::MemoizedCallable<Key, Value, Fn>` wraps a unary callable with an `std::unordered_map`-based cache. The `dsl::memoize<Key, Value>(fn)` factory deduces the callable type and returns a wrapper instance. The call operator looks up the key in the cache, returns the stored value on a hit, and invokes the wrapped callable, stores the result, and returns it on a miss. The cache is a private member, owned by value, with no locking, no eviction, and no instrumentation.

The key type must be hashable by `std::hash` and equality-comparable. Standard integer, pointer, and string types qualify directly; user-defined types require a `std::hash` specialization. Multi-argument memoization is supported by packing arguments into a single key, typically a tuple or pair, for which the user must supply a hasher. The single-argument contract keeps the wrapper minimal and the call syntax transparent.

The wrapper is single-threaded by design, in keeping with the no-threading-library pillar of Chapter 1. Concurrent access is a data race and must be serialized externally, typically by per-thread caches. The wrapper is a value type: copying copies the cache, moving moves it. The `clear_cache()` member empties the cache without destroying the wrapper, and the `Memoization` feature tag exposes the same operation on DSL objects that maintain their own caches.

Memoization is appropriate for expensive pure functions with bounded argument domains, such as recursive numeric functions, recursive AST cost functions, and parser sub-derivation caches. It is inappropriate for impure functions, for functions whose results depend on external state, and for functions whose argument domains are unbounded without external eviction. The most common pitfalls are capturing mutable state in the wrapped lambda, using large argument types as keys, and assuming thread safety where none exists. Used with care, the facility turns exponential overlapping recursion into linear cached evaluation with a few lines of code and no dependencies beyond the standard library.
