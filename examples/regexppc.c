#include <stdio.h>
#include <stdlib.h>

typedef enum {
    TOK_EOF,
    TOK_NUMBER,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_INVALID
} Token;

static const char *cursor;

static Token next_token(void)
{
    for (;;) {
        const char *start = cursor;

        /*!regexppc
            regexppc:yyfill:enable = 0;

            digit = [0-9];
            number = digit+;

            [ \t\r\n]+ { continue; }

            number {
                printf("NUMBER(%.*s)\n",
                    (int)(cursor - start), start);
                return TOK_NUMBER;
            }

            "+" { return TOK_PLUS; }
            "-" { return TOK_MINUS; }
            "*" { return TOK_STAR; }
            "/" { return TOK_SLASH; }
            "(" { return TOK_LPAREN; }
            ")" { return TOK_RPAREN; }

            "\x00" { return TOK_EOF; }

            * {
                fprintf(stderr, "Invalid character: %c\n",
                    *start);
                return TOK_INVALID;
            }
        */
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s 'expression'\n", argv[0]);
        return EXIT_FAILURE;
    }

    cursor = argv[1];

    while (next_token() != TOK_EOF)
        ;

    return EXIT_SUCCESS;
}
