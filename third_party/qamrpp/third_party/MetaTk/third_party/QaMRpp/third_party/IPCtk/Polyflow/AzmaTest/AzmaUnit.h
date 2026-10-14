#ifndef AZMA_UNIT_H
#define AZMA_UNIT_H

/*
 * AzmaUnit.h
 * Tiny header-only test framework for AzmaIDL.
 *
 * Features:
 *   - test registration via macros
 *   - assertion helpers
 *   - optional filtering by substring
 *   - summary reporting
 *   - no external dependencies beyond libc
 *
 * Usage:
 *
 *   #include "Common.h"
 *   #include "AzmaUnit.h"
 *
 *   AZMA_TEST(my_test_name) {
 *       AZMA_EXPECT(1 + 1 == 2);
 *       AZMA_ASSERT_EQ_INT(42, 40 + 2);
 *   }
 *
 *   int main(int argc, char **argv) {
 *       return azma_unit_main(argc, argv);
 *   }
 */

#include "Common.h"

#include <setjmp.h>

/* =========================
   Config
   ========================= */

#ifndef AZMA_UNIT_MAX_TESTS
#  define AZMA_UNIT_MAX_TESTS 2048
#endif

#ifndef AZMA_UNIT_MAX_MESSAGE
#  define AZMA_UNIT_MAX_MESSAGE 1024
#endif

AZMA_EXTERN_C_BEGIN

/* =========================
   Types
   ========================= */

typedef struct AzmaUnitTest AzmaUnitTest;
typedef struct AzmaUnitFailure AzmaUnitFailure;
typedef struct AzmaUnitContext AzmaUnitContext;

typedef void (*AzmaUnitTestFn)(void);

struct AzmaUnitTest {
    const char *name;
    const char *file;
    int line;
    AzmaUnitTestFn fn;
};

struct AzmaUnitFailure {
    const char *expr;
    const char *file;
    int line;
    char message[AZMA_UNIT_MAX_MESSAGE];
};

struct AzmaUnitContext {
    int total;
    int passed;
    int failed;
    int current_failed;
    int assertions;
    const AzmaUnitTest *current_test;
    AzmaUnitFailure last_failure;
    jmp_buf jump_env;
    int can_jump;
    const char *filter;
};

/* =========================
   Registry
   ========================= */

static AzmaUnitTest azma_unit__tests[AZMA_UNIT_MAX_TESTS];
static int azma_unit__test_count = 0;
static AzmaUnitContext azma_unit__ctx = {0};

static AZMA_INLINE int azma_unit_register(
    const char *name,
    const char *file,
    int line,
    AzmaUnitTestFn fn
) {
    AZMA_ASSERT(name != NULL);
    AZMA_ASSERT(file != NULL);
    AZMA_ASSERT(fn != NULL);

    if (azma_unit__test_count >= AZMA_UNIT_MAX_TESTS) {
        fprintf(stderr, "AzmaUnit: too many tests (max=%d)\n", AZMA_UNIT_MAX_TESTS);
        abort();
    }

    azma_unit__tests[azma_unit__test_count].name = name;
    azma_unit__tests[azma_unit__test_count].file = file;
    azma_unit__tests[azma_unit__test_count].line = line;
    azma_unit__tests[azma_unit__test_count].fn = fn;
    azma_unit__test_count++;
    return 0;
}

/* =========================
   Internal helpers
   ========================= */

static AZMA_INLINE int azma_unit__contains(const char *haystack, const char *needle) {
    if (!needle || !needle[0]) return 1;
    if (!haystack) return 0;
    return strstr(haystack, needle) != NULL;
}

static AZMA_INLINE void azma_unit__clear_failure(void) {
    azma_unit__ctx.last_failure.expr = NULL;
    azma_unit__ctx.last_failure.file = NULL;
    azma_unit__ctx.last_failure.line = 0;
    azma_unit__ctx.last_failure.message[0] = '\0';
}

static AZMA_INLINE void azma_unit__fail(
    const char *expr,
    const char *file,
    int line,
    const char *fmt,
    ...
) {
    azma_unit__ctx.current_failed = 1;
    azma_unit__ctx.last_failure.expr = expr;
    azma_unit__ctx.last_failure.file = file;
    azma_unit__ctx.last_failure.line = line;

    if (fmt && fmt[0]) {
        va_list args;
        va_start(args, fmt);
#if AZMA_COMPILER_MSVC
        _vsnprintf(
            azma_unit__ctx.last_failure.message,
            sizeof(azma_unit__ctx.last_failure.message) - 1,
            fmt,
            args
        );
        azma_unit__ctx.last_failure.message[sizeof(azma_unit__ctx.last_failure.message) - 1] = '\0';
#else
        vsnprintf(
            azma_unit__ctx.last_failure.message,
            sizeof(azma_unit__ctx.last_failure.message),
            fmt,
            args
        );
#endif
        va_end(args);
    } else {
        azma_unit__ctx.last_failure.message[0] = '\0';
    }

    if (azma_unit__ctx.can_jump) {
        longjmp(azma_unit__ctx.jump_env, 1);
    }

    abort();
}

static AZMA_INLINE void azma_unit__record_assertion(void) {
    azma_unit__ctx.assertions++;
}

static AZMA_INLINE void azma_unit__print_test_begin(const AzmaUnitTest *test) {
    printf("[ RUN      ] %s\n", test->name);
}

static AZMA_INLINE void azma_unit__print_test_pass(const AzmaUnitTest *test) {
    (void)test;
    printf("[       OK ] %s\n", azma_unit__ctx.current_test->name);
}

static AZMA_INLINE void azma_unit__print_test_fail(const AzmaUnitTest *test) {
    printf("[  FAILED  ] %s\n", test->name);
    if (azma_unit__ctx.last_failure.file) {
        printf("  at %s:%d\n",
               azma_unit__ctx.last_failure.file,
               azma_unit__ctx.last_failure.line);
    }
    if (azma_unit__ctx.last_failure.expr) {
        printf("  expr: %s\n", azma_unit__ctx.last_failure.expr);
    }
    if (azma_unit__ctx.last_failure.message[0]) {
        printf("  note: %s\n", azma_unit__ctx.last_failure.message);
    }
}

static void azma_unit__run_one(const AzmaUnitTest *test) {
    azma_unit__ctx.current_test = test;
    azma_unit__ctx.current_failed = 0;
    azma_unit__clear_failure();

    azma_unit__print_test_begin(test);

    azma_unit__ctx.can_jump = 1;
    if (setjmp(azma_unit__ctx.jump_env) == 0) {
        test->fn();
    }
    azma_unit__ctx.can_jump = 0;

    azma_unit__ctx.total++;

    if (azma_unit__ctx.current_failed) {
        azma_unit__ctx.failed++;
        azma_unit__print_test_fail(test);
    } else {
        azma_unit__ctx.passed++;
        azma_unit__print_test_pass(test);
    }
}

static AZMA_INLINE void azma_unit_print_summary(void) {
    printf("\n");
    printf("Tests run   : %d\n", azma_unit__ctx.total);
    printf("Passed      : %d\n", azma_unit__ctx.passed);
    printf("Failed      : %d\n", azma_unit__ctx.failed);
    printf("Assertions  : %d\n", azma_unit__ctx.assertions);
}

/* =========================
   Public API
   ========================= */

static AZMA_INLINE int azma_unit_run_all(const char *filter) {
    int i;
    azma_unit__ctx.total = 0;
    azma_unit__ctx.passed = 0;
    azma_unit__ctx.failed = 0;
    azma_unit__ctx.assertions = 0;
    azma_unit__ctx.filter = filter;
    azma_unit__ctx.current_test = NULL;
    azma_unit__ctx.can_jump = 0;
    azma_unit__clear_failure();

    for (i = 0; i < azma_unit__test_count; ++i) {
        const AzmaUnitTest *test = &azma_unit__tests[i];
        if (!azma_unit__contains(test->name, filter)) {
            continue;
        }
        azma_unit__run_one(test);
    }

    azma_unit_print_summary();
    return azma_unit__ctx.failed == 0 ? 0 : 1;
}

static AZMA_INLINE int azma_unit_main(int argc, char **argv) {
    const char *filter = NULL;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter = argv[++i];
        } else if (strncmp(argv[i], "--filter=", 9) == 0) {
            filter = argv[i] + 9;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("AzmaUnit options:\n");
            printf("  --filter <substring>   Run only tests whose names contain substring\n");
            printf("  --help                 Show this help\n");
            return 0;
        }
    }

    return azma_unit_run_all(filter);
}

/* =========================
   Assertions
   ========================= */

#define AZMA_FAIL(...) \
    do { \
        azma_unit__record_assertion(); \
        azma_unit__fail("AZMA_FAIL()", __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define AZMA_EXPECT(expr) \
    do { \
        azma_unit__record_assertion(); \
        if (!(expr)) { \
            azma_unit__fail(#expr, __FILE__, __LINE__, "Expectation failed"); \
        } \
    } while (0)

#define AZMA_EXPECT_MSG(expr, ...) \
    do { \
        azma_unit__record_assertion(); \
        if (!(expr)) { \
            azma_unit__fail(#expr, __FILE__, __LINE__, __VA_ARGS__); \
        } \
    } while (0)

#define AZMA_ASSERT_TRUE(expr) AZMA_EXPECT((expr))
#define AZMA_ASSERT_FALSE(expr) AZMA_EXPECT(!(expr))

#define AZMA_ASSERT_EQ_INT(expected, actual) \
    do { \
        int azma__e = (int)(expected); \
        int azma__a = (int)(actual); \
        azma_unit__record_assertion(); \
        if (azma__e != azma__a) { \
            azma_unit__fail( \
                #actual " == " #expected, \
                __FILE__, \
                __LINE__, \
                "expected=%d actual=%d", \
                azma__e, \
                azma__a \
            ); \
        } \
    } while (0)

#define AZMA_ASSERT_EQ_U32(expected, actual) \
    do { \
        uint32_t azma__e = (uint32_t)(expected); \
        uint32_t azma__a = (uint32_t)(actual); \
        azma_unit__record_assertion(); \
        if (azma__e != azma__a) { \
            azma_unit__fail( \
                #actual " == " #expected, \
                __FILE__, \
                __LINE__, \
                "expected=%u actual=%u", \
                (unsigned)azma__e, \
                (unsigned)azma__a \
            ); \
        } \
    } while (0)

#define AZMA_ASSERT_EQ_U64(expected, actual) \
    do { \
        unsigned long long azma__e = (unsigned long long)(expected); \
        unsigned long long azma__a = (unsigned long long)(actual); \
        azma_unit__record_assertion(); \
        if (azma__e != azma__a) { \
            azma_unit__fail( \
                #actual " == " #expected, \
                __FILE__, \
                __LINE__, \
                "expected=%llu actual=%llu", \
                azma__e, \
                azma__a \
            ); \
        } \
    } while (0)

#define AZMA_ASSERT_EQ_STR(expected, actual) \
    do { \
        const char *azma__e = (expected); \
        const char *azma__a = (actual); \
        azma_unit__record_assertion(); \
        if (((azma__e) == NULL) != ((azma__a) == NULL) || \
            ((azma__e) && (azma__a) && strcmp((azma__e), (azma__a)) != 0)) { \
            azma_unit__fail( \
                #actual " == " #expected, \
                __FILE__, \
                __LINE__, \
                "expected=\"%s\" actual=\"%s\"", \
                azma__e ? azma__e : "(null)", \
                azma__a ? azma__a : "(null)" \
            ); \
        } \
    } while (0)

#define AZMA_ASSERT_NE_PTR(a, b) \
    do { \
        const void *azma__a = (const void *)(a); \
        const void *azma__b = (const void *)(b); \
        azma_unit__record_assertion(); \
        if (azma__a == azma__b) { \
            azma_unit__fail(#a " != " #b, __FILE__, __LINE__, "both pointers are %p", azma__a); \
        } \
    } while (0)

#define AZMA_ASSERT_EQ_PTR(a, b) \
    do { \
        const void *azma__a = (const void *)(a); \
        const void *azma__b = (const void *)(b); \
        azma_unit__record_assertion(); \
        if (azma__a != azma__b) { \
            azma_unit__fail(#a " == " #b, __FILE__, __LINE__, "left=%p right=%p", azma__a, azma__b); \
        } \
    } while (0)

#define AZMA_ASSERT_SPAN_EQ(expected_data, expected_size, actual_data, actual_size) \
    do { \
        AzmaSpan azma__e = azma_span_from_parts((expected_data), (expected_size)); \
        AzmaSpan azma__a = azma_span_from_parts((actual_data), (actual_size)); \
        azma_unit__record_assertion(); \
        if (!azma_span_eq(azma__e, azma__a)) { \
            azma_unit__fail( \
                "span equality", \
                __FILE__, \
                __LINE__, \
                "expected_size=%zu actual_size=%zu", \
                azma__e.size, \
                azma__a.size \
            ); \
        } \
    } while (0)

/* =========================
   Test declaration macros
   ========================= */

#define AZMA_TEST(name) \
    static void name(void); \
    static int azma_unit__reg_##name = azma_unit_register(#name, __FILE__, __LINE__, name); \
    static void name(void)

#define AZMA_TEST_NAMED(display_name, fn_name) \
    static void fn_name(void); \
    static int azma_unit__reg_##fn_name = azma_unit_register((display_name), __FILE__, __LINE__, fn_name); \
    static void fn_name(void)

AZMA_EXTERN_C_END

#endif /* AZMA_UNIT_H */
