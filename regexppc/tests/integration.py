"""Generate, compile and execute scanners; no external test framework needed."""
import pathlib
import random
import subprocess
import sys
import tempfile

tool, cc, cxx, root = sys.argv[1:]
root = pathlib.Path(root)


def run(command, **kwargs):
    return subprocess.run(command, check=True, capture_output=True, **kwargs)


with tempfile.TemporaryDirectory(prefix="regexppc-") as directory:
    work = pathlib.Path(directory)
    count = 0

    def scanner(source, data=b"", flex=False, simd=False, arguments=()):
        global count
        count += 1
        spec = work / f"input{count}.txt"
        generated = work / f"scanner{count}.{'cpp' if simd else 'c'}"
        binary = work / f"scanner{count}"
        spec.write_text(source)
        command = [tool, "--flex" if flex else "--re2c"]
        if simd:
            command.append("--simd")
        run(command + [str(spec), "-o", str(generated)])
        run([cxx if simd else cc, "-std=c++17" if simd else "-std=c11",
             "-Wall", "-Wextra", "-Werror", "-O2",
             "-I" + str(root), "-I" + str(root / "third_party/xsimd/include"),
             str(generated), "-o", str(binary)])
        return run([str(binary), *arguments], input=data).stdout

    calculator = (root / "examples/regexppc.c").read_text()
    assert scanner(calculator, arguments=("12+34*(56-7)",)) == b"NUMBER(12)\nNUMBER(34)\nNUMBER(56)\nNUMBER(7)\n"

    flex = r'''%option noyywrap nodefault stack
%x COMMENT
%s INCLUSIVE
digit [0-9]
%{
/* Test nested braces and escaped action strings. */
%}
%%
"if" { puts("keyword"); }
[a-z]+ { printf("word:%s\n", yytext); }
{digit}{2,4} { printf("number:%s\n", yytext); }
"<" { yy_push_state(COMMENT); }
<COMMENT>">" { yy_pop_state(); }
<COMMENT>[^>]+ { }
"@" { BEGIN(INCLUSIVE); }
[ \t\r\n]+ { }
<<EOF>> { return 0; }
%%
int main(void) { yylex(); yylex_destroy(); return 0; }
'''
    expected = b"keyword\nword:iffy\nnumber:1234\nnumber:56\nword:xyz\nword:ifx\n"
    sample = b"if iffy 123456 <ignore all>@xyz ifx"
    assert scanner(flex, sample, flex=True) == expected
    assert scanner(flex, sample, flex=True, simd=True) == expected

    # Regression: alternation arms with loops must have isolated NFA entries.
    alternatives = r'''%%
(a*|b)c { printf("A:%s\n", yytext); }
"ab"+ { printf("B:%s\n", yytext); }
. { printf("X:%s\n", yytext); }
%%
int main(void) { yylex(); yylex_destroy(); return 0; }
'''
    assert scanner(alternatives, b"aabcabab", flex=True) == b"X:a\nB:ab\nA:c\nB:abab\n"

    bounded = r'''#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    const char *cursor = argv[1], *limit = cursor + strlen(cursor);
    while (cursor < limit) {
        const char *start = cursor;
        /*!regexppc
            regexppc:define:YYLIMIT = limit;
            regexppc:lua { define("digit", "[0-9]"); }
            digit+ { printf("N:%.*s\n", (int)(cursor-start), start); }
            [a-z]+ { printf("W:%.*s\n", (int)(cursor-start), start); }
            * { printf("X:%.*s\n", (int)(cursor-start), start); }
        */
    }
    return 0;
}
'''
    rng = random.Random(23)
    text = "9" * 1025 + "a" * 1025 + "".join(rng.choice("abc123! ") for _ in range(1500))
    scalar = scanner(bounded, arguments=(text,))
    assert scanner(bounded, simd=True, arguments=(text,)) == scalar

    lua_flex = r'''%lua { define("digit", "[0-9]"); }
%%
{digit}+ { printf("%d\n", yyleng); }
%%
int main(void) { yylex(); yylex_destroy(); return 0; }
'''
    assert scanner(lua_flex, b"1" * 70000, flex=True, simd=True) == b"70000\n"

    assert scanner('%%\n"a" { }\n%%\nint main(void) { yylex(); yylex_destroy(); }\n', b"x", flex=True) == b"x"
    errors = [
        ("/*!regexppc [ { } */", "unterminated"),
        ("/*!regexppc a = b; b = a; a { } */", "cyclic"),
        ("/*!regexppc missing { } */", "undefined"),
        ('/*!regexppc "a"* { } */', "empty"),
        ('/*!regexppc "a"{4097} { } */', "limit"),
        ('/*!regexppc "a" { } */', "SIMD"),
    ]
    for source, message in errors:
        command = [tool, "-"]
        if message == "SIMD":
            command.insert(1, "--simd")
        result = subprocess.run(command, input=source.encode(), capture_output=True)
        assert result.returncode != 0 and message.encode() in result.stderr, result.stderr
    print(f"Passed: {count} generated scanners, {len(errors)} diagnostic cases")
