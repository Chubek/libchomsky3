// Minimal single-header test runner providing a subset of the Catch2 v2
// macro surface: TEST_CASE, SECTION, REQUIRE, REQUIRE_FALSE,
// REQUIRE_THROWS_AS, REQUIRE_NOTHROW. Standard-library only.

#pragma once
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace satie_test {

inline std::vector<std::pair<std::string, void (*)()>> &registry() {
  static std::vector<std::pair<std::string, void (*)()>> r;
  return r;
}
inline int &passCount() { static int n = 0; return n; }
inline int &failCount() { static int n = 0; return n; }
inline std::string &curTest() { static std::string s; return s; }
inline std::string &curSection() { static std::string s; return s; }

inline void check(bool cond, std::string_view expr, const char *file, int line) {
  if (cond) ++passCount();
  else {
    ++failCount();
    std::cerr << "FAIL: " << curTest();
    if (!curSection().empty()) std::cerr << " / " << curSection();
    std::cerr << "  " << file << ':' << line << "  REQUIRE(" << expr << ")\n";
  }
}

} // namespace satie_test

#define SATIE_CAT_INNER(a, b) a##b
#define SATIE_CAT(a, b) SATIE_CAT_INNER(a, b)

// TEST_CASE_BODY(n) uses a fixed literal n so __COUNTER__ need only be
// resolved once (by the caller). All internal symbols derive from n.
#define TEST_CASE_BODY(n, name)                                                \
  static void SATIE_CAT(satie_tc_, n)();                                       \
  namespace {                                                                  \
  struct SATIE_CAT(satie_reg_, n) {                                            \
    SATIE_CAT(satie_reg_, n)() {                                               \
      ::satie_test::registry().emplace_back(                                   \
          std::string(name), &SATIE_CAT(satie_tc_, n));                        \
    }                                                                          \
  } SATIE_CAT(satie_reg_inst_, n);                                             \
  }                                                                            \
  static void SATIE_CAT(satie_tc_, n)()

#define TEST_CASE(name, ...) TEST_CASE_BODY(__COUNTER__, name)

#define SECTION(name)                                                          \
  for (int SATIE_CAT(satie_sec_, __COUNTER__) =                               \
       (::satie_test::curSection() = (name), 0);                               \
       SATIE_CAT(satie_sec_, __COUNTER__) == 0;                                \
       ++SATIE_CAT(satie_sec_, __COUNTER__))

#define REQUIRE(expr) ::satie_test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define REQUIRE_FALSE(expr) ::satie_test::check(!static_cast<bool>(expr), "!(" #expr ")", __FILE__, __LINE__)
#define REQUIRE_THROWS_AS(expr, type)                                          \
  do {                                                                         \
    bool satie_threw = false;                                                  \
    try { expr; } catch (const type &) { satie_threw = true; }                 \
    catch (...) { satie_threw = false; }                                       \
    ::satie_test::check(satie_threw, #expr " throws " #type, __FILE__, __LINE__); \
  } while (0)
#define REQUIRE_NOTHROW(expr)                                                  \
  do {                                                                         \
    bool satie_ok = true;                                                      \
    try { expr; } catch (...) { satie_ok = false; }                            \
    ::satie_test::check(satie_ok, #expr " nothrow", __FILE__, __LINE__);       \
  } while (0)

#define SATIE_RUN_MAIN                                                        \
  int main() {                                                                \
    for (auto &[name, fn] : ::satie_test::registry()) {                       \
      ::satie_test::curTest() = name;                                         \
      ::satie_test::curSection().clear();                                     \
      try { fn(); }                                                           \
      catch (const std::exception &e) {                                       \
        ++::satie_test::failCount();                                          \
        std::cerr << "UNCAUGHT in " << name << ": " << e.what() << "\n";      \
      }                                                                       \
    }                                                                         \
    std::cout << ::satie_test::passCount() << " assertions passed, "          \
              << ::satie_test::failCount() << " failed\n";                    \
    return ::satie_test::failCount() ? 1 : 0;                                 \
  }
