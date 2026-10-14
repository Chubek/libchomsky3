# Chapter 2: Getting Started: Installation, Build, and First Program

This chapter takes you from an empty directory to a compiled, running DSLtk
program. Because DSLtk is a single header with no dependencies, installation is
mostly a matter of making one file visible to your compiler.

We will cover the requirements, how to obtain and include the header, and how
to compile a program directly with `g++` or `clang++`. We will then look at how
to consume the library through CMake's `INTERFACE` target, and how to build and
run the bundled examples.

Finally, we will walk through the first example program in detail and review
the compile errors you are most likely to meet on day one. By the end you will
have a working DSLtk program and the confidence to read the rest of the manual.

## Requirements

The single hard requirement is a C++20 compiler. DSLtk relies on language
features that did not exist before C++20. It uses concepts, both in real
constraints and in `requires`-expressions.

It uses class-type non-type template parameters, which let `FixedString` serve
as a template argument. It uses `constexpr` lambdas and the brace of
standard-library headers that accompany the modern language.

Any recent release of GCC, Clang, or MSVC that advertises C++20 support will
do. If you are unsure of your toolchain, compile any small program with
`-std=c++20` first. Confirm the toolchain accepts it before reaching for DSLtk.

There are no third-party dependencies. The header includes only standard
library facilities. The full list is `algorithm`, `array`, `concepts`,
`fstream`, `functional`, `memory`, `optional`, `sstream`, `stdexcept`,
`string`, `string_view`, `tuple`, `type_traits`, `unordered_map`, `utility`,
`variant`, and `vector`.

All of these ship with every conforming C++20 implementation. You will never
need to fetch a package, run a package manager, or link a system library to use
DSLtk.

## Obtaining the Header

Obtaining the library means obtaining `DSLtk.hpp`. In a typical source layout
the header sits at the repository root. A project that vendors DSLtk simply
copies that one file into its own include tree.

There is no separate source file to compile, no static library to build, and no
shared object to install for consumers. If your project already has an
`include/` or `third_party/` directory, dropping `DSLtk.hpp` there is
sufficient.

Once the header is on your include path, using it is a single line:

```cpp
#include "DSLtk.hpp"
```

That line pulls the entire `dsl` namespace into your translation unit. Because
the library is header-only, the cost is paid once per translation unit at
compile time. There is no link step and no runtime library.

The `#pragma once` guard at the top of the header prevents double inclusion
within a single translation unit. The include guards of the standard library
headers it pulls in prevent repeated work across the rest of your build.

## A Direct Compiler Build

The simplest possible build is a direct compiler invocation. Save the following
as `hello.cpp` next to a copy of `DSLtk.hpp`:

```cpp
#include "DSLtk.hpp"
#include <iostream>

struct BasicDSL : dsl::DSL<BasicDSL, dsl::Pipeline, dsl::Operators> {};

int main() {
    auto v = BasicDSL::wrap(5)
           | dsl::pipe([](int x) { return x + 3; })
           | dsl::pipe([](int x) { return x * 2; });
    std::cout << v << "\n";   // prints 16
}
```

Compile and run it in one step with GCC:

```sh
g++ -std=c++20 hello.cpp -o hello
./hello
```

With Clang the command is identical in shape:

```sh
clang++ -std=c++20 hello.cpp -o hello
./hello
```

That is the whole build for a consumer. No `-l` flags, no `-I` flags beyond
what your own layout needs, and no `make`. The only mandatory flag is
`-std=c++20` or a newer standard.

Forgetting that flag is the single most common first-day problem. The compiler
will reject concepts and class-type NTTPs with errors that can look
bewildering if you do not know the cause.

If your project keeps headers in a separate directory, point the compiler at it
with `-I`. For example, if `DSLtk.hpp` lives in `include/`:

```sh
g++ -std=c++20 -Iinclude src/hello.cpp -o hello
```

Many DSLtk snippets construct class-type non-type template parameters. On some
toolchains you may benefit from a larger template backtrace limit to keep deep
diagnostics navigable.

```sh
g++ -std=c++20 -ftemplate-backtrace-limit=0 hello.cpp -o hello
```

These are quality-of-life flags, not requirements. They make error messages
easier to read; they do not change whether the code compiles.

## Consuming DSLtk Through CMake

For projects that already use CMake, DSLtk ships a ready-made `INTERFACE`
library target. The distribution's `CMakeLists.txt` declares the target with a
single line and exports the include directory and the C++20 requirement as
usage requirements.

Consumers inherit these requirements automatically:

```cmake
project(DSLtk VERSION 1.0 LANGUAGES CXX)

add_library(DSLtk INTERFACE)

target_include_directories(DSLtk
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(DSLtk INTERFACE cxx_std_20)
```

Because the target is `INTERFACE`, linking against it adds no compiled object
code. It only propagates the include path and the `cxx_std_20` requirement.

A consuming project written against DSLtk therefore needs nothing more than a
few lines of its own:

```cmake
# Assuming DSLtk was added via add_subdirectory, FetchContent, or found
# through the installed package config.
add_executable(hello src/hello.cpp)
target_link_libraries(hello PRIVATE DSLtk)
```

Linking `DSLtk` is enough to bring in both the include directory and the
`-std=c++20` flag. CMake will not let you accidentally build the target with an
older standard, because `target_compile_features(... INTERFACE cxx_std_20)`
makes C++20 a hard usage requirement.

## Installing and Finding the Package

The distribution also installs a package configuration. Downstream projects can
then find DSLtk after it has been installed. The `CMakeLists.txt` exports the
target under the `DSLtk::` namespace and generates a `DSLtkConfig.cmake` from a
template:

```cmake
include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/DSLtkConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DSLtk
)
```

After `cmake --install`, a downstream project can write:

```cmake
find_package(DSLtk REQUIRED)
add_executable(app src/main.cpp)
target_link_libraries(app PRIVATE DSLtk::DSLtk)
```

The generated config runs `check_required_components` and exposes
`DSLtk_INCLUDE_DIR` and `DSLtk_INCLUDE_DIRS` variables. Projects that prefer to
set include paths manually can read those variables directly.

The config template is short. It includes the generated targets file and sets
the include-directory convenience variables from the package prefix. Nothing in
it is project-specific, so it works for any consumer that has installed the
package.

## Building the Bundled Examples

The source distribution includes an `examples/` directory with two dozen
numbered programs. Each one illustrates a single feature or integration. They
are the fastest way to see the library in motion.

The examples are plain `.cpp` files that `#include "DSLtk.hpp"` and define a
`main`. You can compile any one of them directly:

```sh
g++ -std=c++20 examples/01-basic-usage.cpp -o ex01 && ./ex01
g++ -std=c++20 examples/12-expression-templates.cpp -o ex12 && ./ex12
g++ -std=c++20 examples/23-peg-definition.cpp -o ex23 && ./ex23
```

A convenient way to build them all at once is a tiny shell loop. This is also a
handy smoke test for your toolchain:

```sh
mkdir -p build
for f in examples/*.cpp; do
  g++ -std=c++20 "$f" -o "build/$(basename "$f" .cpp)" || echo "failed: $f"
done
```

The examples are numbered to roughly track the manual. `01-basic-usage` matches
the foundations of this chapter. `02-fixed-strings` and `08-ast-creation`
match the AST chapters in Part IV.

The `22-` through `24-` examples correspond to the PEG chapters in Part XII.
Browsing the examples in parallel with the relevant chapter is an effective way
to learn, because each example is small enough to read in full.

## Walking Through the First Example

Let us walk through the very first example in detail, since it establishes
conventions used throughout the manual. `examples/01-basic-usage.cpp` is
reproduced in full below:

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

Line by line: the `#include` pulls in the toolkit. The `struct` declaration
defines an empty DSL that mixes in two features.

`wrap(5)` lifts the integer `5` into the pipeline. The two `dsl::pipe(...)`
stages add three then double. `make_pred` constructs two predicates.

The final line prints the piped value and the conjunction of the two predicate
checks. The pipeline yields `(5 + 3) * 2 = 16`, and `16 > 5 && 16 % 2 == 0` is
true, so the program prints `16 1`.

Notice that `BasicDSL` has an empty body yet still has behaviour. The pipeline
feature injects the static `wrap` method. The operators feature injects the
static `make_pred` factory.

The free-function `operator|` in the `dsl` namespace handles the pipe. This
separation — features contribute methods, free functions contribute operators —
recurs throughout the library. Chapters 3 and 5 explore it fully.

## A Second Example: Expression Templates

A second example worth building immediately is the expression-template demo,
`examples/12-expression-templates.cpp`. It defines a `Scalar` type that
inherits `dsl::ExprTemplates`.

The type stores a `double` and exposes `expr_value()`. The arithmetic operators
are then provided by the mixin:

```cpp
struct Scalar : dsl::DSL<Scalar, dsl::ExprTemplates> {
  double v{};
  Scalar() = default;
  explicit Scalar(double x) : v(x) {}
  double expr_value() const { return v; }
};

int main() {
  Scalar a{2}, b{3}, c{4};
  auto expr = a + b * c - a;
  std::cout << expr.eval() << "\n";   // 2 + 3*4 - 2 = 12
}
```

Compiling and running this confirms that the expression-template feature built
a lazy tree from `a + b * c - a`. The call to `.eval()` then reduced it to `12`.

Chapter 15 and Chapter 16 explain the mechanism. For now, treat the example as
proof that the toolkit compiles and runs on your machine.

## A Third Example: The PEG Engine

A third example, `examples/23-peg-definition.cpp`, exercises the heaviest
subsystem — the Parsing Expression Grammar engine. It creates a grammar, adds
rules with compile-time pattern strings, and parses a sentence.

Whitespace is routed to an ignore channel so it is matched but not reported as
a token:

```cpp
auto grammar = dsl::create_peg_definition();
auto &ws = grammar.add_rule<"[ \\t\\n]+">([](dsl::PEGMatch &m){
    std::cout << "skip_ws:" << m.length() << "\n";
});
ws.channel = dsl::PEGIgnoreChannel;
auto &kw_if = grammar.add_rule<"if">([](dsl::PEGMatch &m){
    std::cout << "kw:" << m.value << "\n";
});
auto result = grammar.parse("if x1 + 2 then 40");
std::cout << (result.ok() ? "parse_ok" : "parse_failed") << "\n";
```

Building this example is a good smoke test that your toolchain's C++20 support
is complete enough for the whole library. The PEG engine exercises concepts,
variadic templates, and `constexpr` pattern compilation.

Parts X through XII cover the parser and grammar machinery in depth. If this
example compiles and runs, everything in between will too.

## Common Build Problems

When a build fails, the cause is almost always one of a small number of
things. The first is the standard version. If you see errors about `concepts`,
`requires`, or `std::same_as`, you have forgotten `-std=c++20`.

The second is the include path. If the compiler reports `DSLtk.hpp: No such
file or directory`, the header is not where the compiler is looking. Add the
right `-I` flag or fix the `target_include_directories` entry.

The third is an old compiler. A toolchain that predates solid class-type NTTP
support will choke on `dsl::leaf<"tag">` and `dsl::pattern<"...">` even with
`-std=c++20`. The fix is to upgrade to a current release of your compiler.

A subtler class of error involves template diagnostics. Because DSLtk is
template-heavy, a mistake in your own code can produce a wall of instantiation
traceback. Two habits help you cope.

First, build the smallest example that reproduces the problem in isolation,
away from your real codebase. Second, read the *first* error in the output, not
the last.

The first error is usually the root cause. Everything after it is cascading
fallout. The bundled examples give you a known-good baseline to diff against
when something goes wrong.

## A Productive Learning Workflow

Another useful habit is to keep a throwaway translation unit that you compile
often while learning. DSLtk's features are independent, so you can grow this
file one feature at a time.

Add the pipeline today, the operators tomorrow, and pattern matching the day
after. Confirm each compiles before stacking the next. This incremental
approach matches the manual's part structure and keeps feedback loops short.

If you integrate DSLtk into a larger project, consider wrapping it behind your
own thin header. Do not include `DSLtk.hpp` directly from many places.

The library's own overview comment suggests exactly this. Define your DSL type
in `mydsl.hpp`, include `DSLtk.hpp` there, and have the rest of your code
include only `mydsl.hpp`.

```cpp
// mydsl.hpp — the only file that sees DSLtk.hpp directly
#include "DSLtk.hpp"

struct MathDSL : dsl::DSL<MathDSL, dsl::Pipeline, dsl::Operators> {
    // domain-specific surface here
};
```

This localises the toolkit. It lets you adjust your DSL's surface without
touching callers, and it keeps your build's include graph clean.

## Build Performance

Build performance is rarely a concern for DSLtk itself. The header is modest in
size, and the templates it instantiates are shallow.

The expression templates and parser combinators are the most
instantiation-heavy parts. If compile time matters in a large project, factor
heavy DSL usage into a few translation units rather than spreading it across
many.

For typical use, the difference is imperceptible. A program that includes the
header and uses a couple of features compiles in well under a second on modern
hardware.

## Summary

The workflow is simple. Put `DSLtk.hpp` on your include path, compile with
`-std=c++20`, and link nothing. If you use CMake, link the `DSLtk` `INTERFACE`
target and inherit its usage requirements.

Build a bundled example first to confirm the toolchain, then write your own
`dsl::DSL` subclass. The next chapter opens the black box of that base class.

You will see how CRTP and the feature mixins turn one line of inheritance into
a fully featured language. By now, though, you should have a compiled DSLtk
program running on your machine and a feel for the three-step loop introduced
in Chapter 1: inherit `dsl::DSL`, add your domain members, and express logic
through the operators those features provide.

Everything that follows in the manual adds depth to that loop, one subsystem at
a time. If your first program ran, you are ready to begin.
