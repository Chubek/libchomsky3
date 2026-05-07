/**
 * libchomsky3 - Lightweight parser lifecycle API
 */

#include "chomsky3/chomsky3.h"
#include <stdlib.h>
#include <string.h>

static char *parser_strdup(const char *str) {
    size_t length = strlen(str);
    char *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, str, length + 1);
    }
    return copy;
}

struct chomsky3_parser {
    char *grammar;
    chomsky3_context_t *ctx;
    chomsky3_pattern_t *pattern;
    chomsky3_options_t options;
    char *error_message;
};

static void set_error(chomsky3_parser *parser, const char *message) {
    if (!parser) {
        return;
    }
    free(parser->error_message);
    parser->error_message = message ? parser_strdup(message) : NULL;
}

chomsky3_parser *chomsky3_parser_new(const char *grammar, const chomsky3_options_t *opts) {
    if (!grammar) {
        return NULL;
    }

    chomsky3_parser *parser = calloc(1, sizeof(*parser));
    if (!parser) {
        return NULL;
    }

    parser->grammar = parser_strdup(grammar);
    if (!parser->grammar) {
        free(parser);
        return NULL;
    }

    parser->ctx = chomsky3_context_new();
    if (!parser->ctx) {
        free(parser->grammar);
        free(parser);
        return NULL;
    }

    if (opts) {
        parser->options = *opts;
    } else {
        parser->options.max_steps = 0;
        parser->options.max_backtrack = 0;
        parser->options.max_stack_depth = 0;
        parser->options.enable_jit = 1;
    }

    chomsky3_error_t err = CHOMSKY3_OK;
    parser->pattern = chomsky3_compile(parser->ctx,
                                      parser->grammar,
                                      CHOMSKY3_TARGET_BYTECODE,
                                      0,
                                      &err);
    if (!parser->pattern || err != CHOMSKY3_OK) {
        set_error(parser, "failed to compile grammar/pattern");
        if (parser->pattern) {
            chomsky3_pattern_free(parser->pattern);
            parser->pattern = NULL;
        }
        chomsky3_context_free(parser->ctx);
        free(parser->grammar);
        free(parser);
        return NULL;
    }

    return parser;
}

int chomsky3_parse(chomsky3_parser *parser, const char *input, size_t len) {
    if (!parser || !input) {
        if (parser) {
            set_error(parser, "invalid parser or input");
        }
        return -1;
    }

    set_error(parser, NULL);

    chomsky3_match_t match;
    bool ok = chomsky3_exec(parser->pattern, input, len, &match);
    if (!ok) {
        set_error(parser, "parse failed");
        return -1;
    }

    if (match.start) {
        free(match.groups);
    }

    return 0;
}

void chomsky3_parser_free(chomsky3_parser *parser) {
    if (!parser) {
        return;
    }

    if (parser->pattern) {
        chomsky3_pattern_free(parser->pattern);
    }
    if (parser->ctx) {
        chomsky3_context_free(parser->ctx);
    }
    free(parser->grammar);
    free(parser->error_message);
    free(parser);
}

const char *chomsky3_error_message(chomsky3_parser *parser) {
    if (!parser) {
        return NULL;
    }

    return parser->error_message ? parser->error_message : "";
}
