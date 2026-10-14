# Chapter 26: Task Pipelines: `Task`, `TaskChain`, `TaskState`

The Task pipeline feature provides DSLtk's answer to a recurring need in embedded DSLs: describing a sequence of staged computations that run in order, can suspend themselves, and can resume later. Unlike the value-level pipelines of Chapter 6, which thread a value through pure transformations, task pipelines are concerned with units of work that have names, side effects, and explicit state.

A task pipeline is built from three cooperating pieces. `TaskState` holds the shared mutable context that every task reads and writes. `Task` is a single unit of work, pairing a name with a callable. `TaskChain` is an ordered sequence of `Task` objects composed with the `|` operator. Three free functions — `run`, `suspend`, and `upon` — drive and instrument the chain.

The execution model is cooperative and single-threaded. There are no preemptive threads, no worker pool, no scheduler ticking at fixed intervals. A task runs until it either finishes or calls `suspend`; the chain then either advances to the next task or stops until the program calls `run` again. This ties directly to the single-threaded design pillar introduced in Chapter 1.

## The Cooperative Execution Model

Cooperative scheduling places the responsibility for fairness on the task itself. The runtime never interrupts a task that is in the middle of its work; the task must explicitly yield by setting the suspended flag. This is the same discipline that underlies coroutines, generators, and event loops in many language runtimes, but DSLtk expresses it with a single boolean field rather than a full coroutine machinery.

The practical consequence is that a long-running task blocks the entire chain. If a task spins in a tight loop or performs blocking I/O, no other task will run until it returns. The programmer contracts to keep tasks short, or to break long work into multiple tasks that cooperate via `suspend`.

Because the model is single-threaded, tasks do not need locks on `TaskState`. Every access to the shared map happens in the same call stack, sequentially. This is a deliberate simplification: it keeps the header dependency-free and makes task pipelines predictable to reason about.

The chain does not run on its own. It only advances when `run` is called. A program can build a chain, run it partway, inspect the state, react to it, and then run it further. This is the suspension and resumption pattern that distinguishes a task pipeline from a plain function call.

Contrast this with the value pipes of Chapter 6. A value pipe is a pure expression: input flows in one end, output emerges from the other, and there is no observable intermediate state. A task chain, by contrast, exists precisely so that intermediate state is observable, named, and addressable by later tasks and by external hooks.

## TaskState: Shared Mutable State

`TaskState` is the shared blackboard on which tasks communicate. Its definition is intentionally minimal.

```cpp
struct TaskState
{
  std::unordered_map<std::string, Result<std::string, std::string>> results{};
  bool suspended{ false };
};
```

The `results` map is keyed by task name and stores the `Result<std::string, std::string>` that each task returns. This is the primary channel of communication: a task writes its outcome under its own name, and later tasks read earlier outcomes by name. The use of `Result` (Chapter 22) means that both success values and error messages live in the same structure.

The `suspended` flag is the cooperative yield signal. It is `false` at construction, set to `true` by `suspend`, and read by `run` after each task to decide whether to continue. Because it lives on the state rather than on the chain, a suspended chain can be inspected, logged, or modified before it is resumed.

The choice of `std::unordered_map` reflects the addressing pattern. Tasks almost always look up earlier results by name rather than iterating, so hash-based access is the right default. The map is also the natural place for hooks to deposit side outputs, as the `upon` helper demonstrates.

Note that `TaskState` carries no type parameter. Values exchanged between tasks are strings. This keeps the pipeline composable at the type level — any task can read any other task's result without templated fan-out — at the cost of requiring tasks to marshal their data to and from `std::string`. The `Result` type carries the error channel, so a failed task's name maps to an error message rather than a value.

## The Task Unit of Work

A `Task` is a named callable. Its definition pairs a `std::string` name with a `std::function` whose signature is fixed by the state contract.

```cpp
struct Task
{
  std::string name{};
  std::function<Result<std::string, std::string> (TaskState &)> run_fn{};
  Result<std::string, std::string>
  run (TaskState &state) const
  {
    return run_fn (state);
  }
};
```

Every task callable takes a `TaskState&` and returns a `Result<std::string, std::string>`. The state argument is how the task reads upstream results and how it writes its own. The return value is what `run` stores under the task's name in `state.results`.

Tasks are constructed with aggregate initialization. The first field is the name, the second is the callable. The name is significant: it is the key under which the result is stored, and it is the handle that later tasks and `upon` hooks use to address this task's output.

A task is not a coroutine. It is an ordinary callable that runs to completion synchronously. Suspension is achieved not by pausing the callable mid-execution, but by having the callable set `state.suspended = true` (directly or via `suspend`) before returning. The chain then stops after this task and control returns to the caller of `run`.

Because the callable is stored in a `std::function`, tasks own their captured state by value. Lambdas that capture local context — configuration, buffers, callbacks — are the normal way to build a task. The captured state lives as long as the task, and hence as long as the chain that contains it.

## TaskChain: Composing Tasks with operator|

A `TaskChain` is the ordered container for tasks. It holds a `std::vector<Task>` and a `TaskPolicy`, and it is built by overloading the `|` operator.

```cpp
struct TaskChain
{
  std::vector<Task> tasks{};
  TaskPolicy policy{ TaskPolicy::StopOnError };
};
```

Composition uses two overloads. The first joins two `Task` objects into a chain; the second appends a `Task` to an existing chain.

```cpp
inline TaskChain
operator| (Task lhs, Task rhs)
{
  return TaskChain{ { std::move (lhs), std::move (rhs) },
                    TaskPolicy::StopOnError };
}

inline TaskChain
operator| (TaskChain lhs, Task rhs)
{
  lhs.tasks.push_back (std::move (rhs));
  return lhs;
}
```

The `|` operator was chosen deliberately to echo the value pipelines of Chapter 6. The visual symmetry is intentional: both features compose stages left to right. The semantics differ — value pipes thread a value, task chains thread a stateful blackboard — but the surface syntax is uniform across the toolkit.

A chain is a value type. Passing it around, returning it, or storing it copies the vector of tasks. Because tasks own their captured state, copying a chain copies that state too. For most uses a chain is built once at the call site of `run` and not copied afterward.

## TaskPolicy: Error Propagation

The `TaskPolicy` enum controls what happens when a task returns an error.

```cpp
enum class TaskPolicy
{
  StopOnError,
  ContinueOnError
};
```

Under `StopOnError` — the default — `run` breaks out of its loop as soon as a task returns an `err` result. Later tasks do not execute. The error is recorded in `state.results` under the failing task's name, and the caller of `run` can inspect it.

Under `ContinueOnError`, `run` does not break on errors. Every task executes regardless of prior failures. This is useful when tasks are independent cleanup steps, or when the chain is gathering a set of results whose individual success or failure is reported in aggregate.

The policy is a property of the chain, not of the state. The same `TaskState` can be driven by chains with different policies. The policy is set when the chain is constructed; the `operator|` overloads initialize it to `StopOnError`, and a program that wants `ContinueOnError` must assign it explicitly after construction.

## The run Helper: Advancing a Chain

The `run` free function is the engine of the pipeline. Its definition is short and worth quoting verbatim.

```cpp
template <typename... Args>
TaskState &
run (TaskState &state, const TaskChain &chain, Args &&...)
{
  for (const auto &task : chain.tasks)
    {
      auto result = task.run (state);
    state.results.insert_or_assign (task.name, result);
      if (result.is_err () && chain.policy == TaskPolicy::StopOnError)
        break;
      if (state.suspended)
        break;
    }
  return state;
}
```

Each task in the chain is invoked in order. Its result is stored under its name with `insert_or_assign`, which means running the same chain twice will overwrite prior results. Two termination conditions are checked after each task: an error combined with `StopOnError` policy, and the `suspended` flag.

The variadic `Args&&...` tail is a deliberate extension point. It allows callers to pass additional arguments through `run` without the header having to anticipate them; the arguments are accepted and discarded by the variadic pack. This keeps `run` forward-compatible with richer scheduling hooks.

`run` returns a reference to the state it was given. This allows the call to be embedded in a larger expression, for example passing the post-run state straight to an inspection routine. The return type also documents that `run` does not copy the state; it mutates it in place.

A chain that has been suspended can be advanced further by calling `run` again on the same state and chain. Because `run` always iterates from the beginning of the chain, the program is responsible for clearing `state.suspended` before resuming, and for ensuring that already-completed tasks tolerate being re-run, or for trimming the chain. A common idiom is to rebuild a shorter chain containing only the remaining tasks before resuming.

## The suspend Helper: Yielding Mid-Task

`suspend` is the cooperative yield primitive.

```cpp
inline void
suspend (TaskState &state)
{
  state.suspended = true;
}
```

A task calls `suspend(state)` when it cannot complete its work in this turn — for example, because it has issued an asynchronous request and must wait for the response. The flag is set, the task returns a `Result` (often an intermediate or placeholder), and `run` breaks out of its loop after this task.

Suspension is a signal, not a continuation. There is no stored resumption point; the task's callable has already returned by the time `run` observes the flag. Resuming means calling `run` again, which re-enters the chain from the top. The task that suspended is expected to detect, on re-entry, that its earlier work is recorded in `state.results` or in external state it captured, and to skip or continue accordingly.

This design avoids the machinery of coroutines while preserving the cooperative scheduling discipline. The cost is that the programmer must encode resumption logic explicitly, typically by checking whether an upstream or external result is already present before performing the suspended work again.

Because the flag lives on the state, suspension is observable. A driver loop can poll `state.suspended` to decide whether to sleep, to perform other work, or to wake the chain. The flag is the only contract between the task and the driver.

## The upon Helper: Event-Conditioned Hooks

`upon` registers a hook that fires when an event predicate becomes true. Its definition is compact.

```cpp
template <typename EventFn>
void
upon (TaskState &state, EventFn &&event, Task task)
{
  if (event ())
    state.results[task.name] = task.run (state);
}
```

The hook is a pair: a predicate callable `event` and a `Task`. When `upon` is called, the predicate is evaluated immediately. If it returns `true`, the task is run and its result is stored under the task's name, exactly as `run` would store it.

The predicate typically inspects `state.results` to decide whether its condition holds. Because the state map is the canonical record of what has happened, predicates are written as queries against that map. The example shipped with the toolkit checks whether the `inspect` task succeeded and produced the value `"even"` before running the `normalize` task.

`upon` is a single-shot check, not a persistent subscription. It evaluates its predicate once, at the moment it is called. To implement continuous monitoring, a driver loop must call `upon` repeatedly, or the program must call `upon` at the points where the event could have occurred — typically after each `run`.

The ordering rule follows from this: `upon` hooks must be registered after the events they observe have had a chance to occur. Registering a hook before `run` and expecting it to fire retroactively is a mistake; the predicate is evaluated at registration time, not deferred. A program that wants a hook to fire whenever a task completes should call `upon` immediately after the relevant `run`.

## Value and Context Flow Between Tasks

The pipeline does not thread a typed value from stage to stage. Instead, every task reads from and writes to the shared `results` map. This is the central consequence of the `TaskState` design.

A task accesses upstream output by looking up the upstream task's name in `state.results`. The lookup yields a `Result<std::string, std::string>`, which the task must check for `is_ok` before unwrapping. A common pattern is to return an `err` result immediately if an upstream result is missing or failed, propagating the failure forward under the chain's policy.

The output of a task is its return value, stored under its own name. Tasks do not write into other tasks' names directly; they write their own result and let later tasks read it. This naming discipline keeps the data flow readable and prevents accidental aliasing.

Because values are strings, tasks that work with structured data must marshal it. A task that produces an integer will convert it to a string on output; a downstream task will parse it back. The cost is acceptable for the orchestration layer that task pipelines occupy, and it keeps the types uniform across heterogeneous stages.

The `Result` wrapper means that the error channel is always available. A task that cannot proceed returns `err` with a message; the chain's policy decides whether later tasks run. This integrates with the error-flow discipline of Chapter 22 without requiring the pipeline to be templated on the value type.

## A Three-Stage Pipeline: Load, Transform, Store

The canonical task pipeline is a load-transform-store sequence. Each stage is a `Task`, and they are composed with `|`.

```cpp
#include "DSLtk.hpp"
#include <iostream>

int main()
{
  using Result = dsl::Result<std::string, std::string>;

  auto load = dsl::Task{
      "load",
      [](dsl::TaskState &state) {
        state.results["source"] = Result::from_ok("42");
        return Result::from_ok("loaded");
      }};

  auto transform = dsl::Task{
      "transform",
      [](dsl::TaskState &state) {
        auto it = state.results.find("source");
        if (it == state.results.end() || it->second.is_err())
          return Result::from_err("no source");
        int v = std::stoi(it->second.unwrap());
        return Result::from_ok(std::to_string(v * 2));
      }};

  auto store = dsl::Task{
      "store",
      [](dsl::TaskState &state) {
        auto it = state.results.find("transform");
        if (it == state.results.end() || it->second.is_err())
          return Result::from_err("nothing to store");
        return Result::from_ok("stored:" + it->second.unwrap());
      }};

  auto chain = load | transform | store;
  dsl::TaskState state{};
  dsl::run(state, chain);

  for (const auto &entry : state.results)
    std::cout << entry.first << " -> "
              << (entry.second.is_ok() ? entry.second.unwrap_or("?")
                                       : entry.second.unwrap_or("?"))
              << "\n";
}
```

The `load` task seeds the state with a source value. The `transform` task reads that source, doubles it, and returns the result under its own name. The `store` task reads the transformed value and returns a confirmation string. Each stage's name is the key under which downstream stages find its output.

Note how each task defensively checks for the presence and success of its upstream. This is the pipeline's equivalent of null-checking: because the map is shared and mutable, a task cannot assume its predecessor ran. The defensive check is what makes tasks composable in arbitrary orders.

The final loop prints every recorded result. The map contains entries for `source` (written by `load`), `load`, `transform`, and `store`. This illustrates a useful property: tasks can deposit side results under arbitrary keys, not only their own name, which lets a single task publish multiple outputs.

## Suspending on a Simulated Async Boundary

Suppose the `load` stage must wait for an external event that is not yet available. The task can `suspend`, returning control to the driver, which later calls `run` again.

```cpp
using Result = dsl::Result<std::string, std::string>;

dsl::TaskState state{};
bool external_ready = false;

auto load = dsl::Task{
    "load",
    [&](dsl::TaskState &state) {
      if (!external_ready) {
        dsl::suspend(state);
        return Result::from_ok("pending");
      }
      state.results["source"] = Result::from_ok("42");
      return Result::from_ok("loaded");
    }};

auto echo = dsl::Task{
    "echo",
    [](dsl::TaskState &state) {
      auto it = state.results.find("source");
      if (it == state.results.end() || it->second.is_err())
        return Result::from_err("no source");
      return Result::from_ok(it->second.unwrap());
    }};

auto chain = load | echo;
dsl::run(state, chain);          // load suspends, echo does not run
assert(state.suspended);

external_ready = true;
state.suspended = false;
dsl::run(state, chain);          // load now completes, echo runs
```

On the first call to `run`, `load` finds the external flag false, calls `suspend`, and returns a placeholder. `run` observes the flag and breaks before `echo`. The driver then sets the external flag, clears the suspended flag, and calls `run` again. On the second pass `load` runs to completion, `echo` runs, and the chain finishes.

This pattern demonstrates the discipline required of resumable chains. Because `run` re-enters from the top, the `load` task must be idempotent in its suspended state: re-running it before the external flag is set must produce the same suspension, and re-running it after must complete. The check against `external_ready` provides exactly this idempotence.

The second pass re-runs `load` from scratch. Any side effect that `load` performed on its first pass — for example, writing to `state.results` — must either be safe to repeat or guarded. In this example the first pass writes nothing to the map, so re-running is harmless.

## Registering upon Hooks for Logging

`upon` is well suited to conditional logging or conditional side tasks. The shipped example uses it to run a `normalize` task only when the `inspect` task produced a specific value.

```cpp
dsl::Task normalize{
    "normalize",
    [](dsl::TaskState &) {
      return Result::from_ok("normalized");
    }};

dsl::upon(
    state,
    [&state] {
      return state.results.count("inspect")
             && state.results.at("inspect").is_ok()
             ? state.results.at("inspect").unwrap() == "even"
             : false;
    },
    normalize);
```

The predicate checks that `inspect` succeeded and that its value is `"even"`. Only then does `normalize` run. If the predicate is false, `normalize` is not executed and no entry is written under its name.

A logging hook can use the same mechanism with a task whose side effect is output rather than computation.

```cpp
dsl::Task log_complete{
    "log",
    [](dsl::TaskState &state) {
      std::cout << "pipeline reached: ";
      for (const auto &e : state.results)
        std::cout << e.first << " ";
      std::cout << "\n";
      return Result::from_ok("logged");
    }};

dsl::upon(state, [&state] { return state.results.count("store") > 0; },
          log_complete);
```

Here the predicate fires once the `store` task has recorded a result. The hook runs a task that prints the names of all completed stages. The ordering rule is respected: `upon` is called after `run`, so the predicate observes the post-run state.

Multiple `upon` calls can be chained to layer several conditional side tasks. Each evaluates its predicate independently against the same state. The order in which they are called determines the order in which their tasks execute, and a task run by an earlier `upon` is visible to the predicates of later ones because both write into the same `state.results` map.

## A Build Pipeline of Dependent Steps

Task pipelines model build systems well, because build steps are explicitly ordered, explicitly named, and frequently conditional. A small build pipeline might configure, compile, and archive.

```cpp
using Result = dsl::Result<std::string, std::string>;

auto configure = dsl::Task{
    "configure",
    [](dsl::TaskState &s) {
      s.results["config"] = Result::from_ok("release");
      return Result::from_ok("configured");
    }};

auto compile = dsl::Task{
    "compile",
    [](dsl::TaskState &s) {
      auto it = s.results.find("config");
      if (it == s.results.end() || it->second.is_err())
        return Result::from_err("configure failed");
      s.results["artifact"] = Result::from_ok("app.bin");
      return Result::from_ok("compiled");
    }};

auto archive = dsl::Task{
    "archive",
    [](dsl::TaskState &s) {
      auto it = s.results.find("artifact");
      if (it == s.results.end() || it->second.is_err())
        return Result::from_err("no artifact");
      return Result::from_ok("app.tar");
    }};

auto build = configure | compile | archive;
dsl::TaskState st{};
dsl::run(st, build);
```

Each step depends on a named output of its predecessor. `compile` reads `config`; `archive` reads `artifact`. If `configure` failed, the `StopOnError` policy would prevent `compile` from running, and `compile`'s defensive check would also catch the absence of `config` if the policy were changed to `ContinueOnError`.

The defensive checks matter because they decouple the task from the policy. A task written to check its inputs will behave correctly under either policy, while a task that assumes its predecessor ran will only behave correctly under `StopOnError`. Writing the checks is the more robust style.

A conditional step — for example, publishing only when the build is a release — fits naturally as an `upon` hook after the chain runs. The predicate inspects `config`, and the publish task runs only when the configuration is `release`. This keeps the main chain linear and moves the conditional logic to the side.

## Error Propagation in Practice

The two policies produce different post-conditions. Under `StopOnError`, the `results` map contains entries only up to and including the first failing task. Under `ContinueOnError`, it contains an entry for every task in the chain.

```cpp
using Result = dsl::Result<std::string, std::string>;

auto a = dsl::Task{"a", [](dsl::TaskState &) {
                       return Result::from_ok("ok");
                     }};
auto b = dsl::Task{"b", [](dsl::TaskState &) {
                       return Result::from_err("boom");
                     }};
auto c = dsl::Task{"c", [](dsl::TaskState &) {
                       return Result::from_ok("ok");
                     }};

dsl::TaskState s1{};
auto stop_chain = a | b | c;
stop_chain.policy = dsl::TaskPolicy::StopOnError;
dsl::run(s1, stop_chain);
// s1.results contains "a" and "b"; "c" did not run.

dsl::TaskState s2{};
auto cont_chain = a | b | c;
cont_chain.policy = dsl::TaskPolicy::ContinueOnError;
dsl::run(s2, cont_chain);
// s2.results contains "a", "b", and "c".
```

Choosing between the policies is a question of whether later tasks can usefully run after an earlier failure. A pipeline whose stages are independent reports benefits from `ContinueOnError`; a pipeline whose stages are strictly dependent benefits from `StopOnError`, the default.

A task that wants to suppress an error and let the chain continue can return `ok` with a marker value instead of `err`, leaving the policy to govern only genuine failures. This is the usual way to record a non-fatal warning without halting the chain.

## Combining Pipelines with Other DSLtk Features

Task pipelines compose with the rest of the toolkit. A task's callable can invoke a parser (Chapter 23), drive a rewrite (Chapter 13), or evaluate a lazy expression (Chapter 19). The stringly-typed result map is the integration boundary: richer values produced by other features are marshalled to strings on the way in and parsed back out on the way out.

For example, a task that runs a parser on its input might store the parsed representation as a string for the next task to consume, or might store only a summary and keep the structured value in captured state. The latter pattern is common when the structured value is large or expensive to re-parse.

The lazy values of Chapter 19 are a useful complement to suspension. A task that suspends waiting for external input can store a `Lazy<T>` in its captured state and force it on resumption. This separates the description of the deferred work from the scheduling of when it runs, which is exactly the concern of the task pipeline.

Contrast this with parser combinators (Chapter 23), which are themselves a kind of staged computation. Parsers stage their work through recursive descent, not through a cooperative scheduler; they do not suspend and resume. Task pipelines are the feature to reach for when the staging is temporal — when work happens in turns separated by external events — rather than structural.

## Pitfalls and Common Mistakes

The first pitfall is a task that neither completes nor suspends. A task that loops forever, or that blocks on I/O without calling `suspend`, stalls the chain indefinitely. Because the model is cooperative and single-threaded, no other task will preempt it. The remedy is to break long work into tasks that suspend explicitly.

The second pitfall is registering `upon` hooks before `run` and expecting them to fire. The predicate is evaluated at the moment `upon` is called, not deferred. A hook meant to observe a task's completion must be registered after that task has had a chance to run — typically immediately after the `run` call.

The third pitfall is re-running a suspended chain without clearing `state.suspended`. Because `run` checks the flag after each task, leaving it set causes the chain to stop after the first task on every subsequent call. The driver is responsible for clearing the flag, and for ensuring that any task whose suspension set the flag is prepared to be re-entered.

The fourth pitfall is assuming a task's predecessor ran. Under `ContinueOnError`, or after a suspension, a task may find its upstream absent or failed. Defensive checks against `state.results` are the correct defense, and they make tasks robust to reordering and policy changes.

The fifth pitfall is relying on a specific iteration order of `state.results`. The map is an `unordered_map`, so iteration order is not the insertion order. Tasks that need ordered output must sort the keys themselves, or maintain a separate ordered list of names in captured state.

The sixth pitfall is writing to another task's name. While the map allows it, doing so destroys the naming discipline that makes pipelines readable. Each task should write only its own result, plus any explicitly side outputs under clearly side-channel keys. Downstream tasks read by name; overwriting another task's name silently corrupts that contract.

The seventh pitfall is ignoring the cost of string marshalling. Because values are strings, a pipeline that shuffles large structured data between tasks pays a marshalling cost at every boundary. For orchestration this is rarely significant, but for tight inner loops the value pipelines of Chapter 6 are the better tool.

The eighth pitfall is confusing task chains with value pipes. They share the `|` operator but differ in semantics. Value pipes are pure and stateless; task chains are stateful and side-effecting. Reaching for a task chain when a value pipe suffices introduces unnecessary state, and reaching for a value pipe when stateful coordination is needed forces the state into external variables.

## Summary

The Task pipeline feature gives DSLtk a cooperative, single-threaded staging mechanism. `TaskState` is the shared blackboard — a name-to-result map plus a suspended flag. `Task` is a named callable that reads from and writes to that blackboard. `TaskChain` composes tasks with the `|` operator and carries an error policy. `run` advances the chain one task at a time, stopping on error or suspension. `suspend` yields control back to the driver by setting a flag. `upon` registers a single-shot conditional hook that runs a task when a predicate over the state holds.

The model is deliberately simple: no threads, no locks, no scheduler. Tasks are responsible for keeping themselves short and for encoding their own resumption logic. The stringly-typed result map is the integration boundary with the rest of the toolkit, and the `Result` wrapper provides the error channel. Used within these constraints, task pipelines are a readable way to orchestrate staged, possibly suspending computations — complementing the pure value pipes of Chapter 6, the lazy values of Chapter 19, and the structural staging of the parser combinators of Chapter 23.
