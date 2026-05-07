/** Minimal regex AST implementation. */

#include "chomsky3/regex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *dup_range(const char *pattern, size_t length) {
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, pattern, length);
    copy[length] = '\0';
    return copy;
}

static chomsky3_regex_node_t *node_new(chomsky3_node_type_t type) {
    chomsky3_regex_node_t *node = calloc(1, sizeof(*node));
    if (node) {
        node->type = type;
        node->greedy = true;
    }
    return node;
}

chomsky3_regex_t *chomsky3_regex_new(const char *pattern, size_t length) {
    if (!pattern) return NULL;
    chomsky3_regex_t *regex = calloc(1, sizeof(*regex));
    if (!regex) return NULL;
    regex->pattern = dup_range(pattern, length);
    if (!regex->pattern) {
        free(regex);
        return NULL;
    }
    regex->pattern_length = length;
    regex->root = length ? chomsky3_node_literal((unsigned char)pattern[0]) : NULL;
    return regex;
}

void chomsky3_regex_free(chomsky3_regex_t *regex) {
    if (!regex) return;
    chomsky3_node_free(regex->root);
    free(regex->pattern);
    free(regex);
}

chomsky3_regex_node_t *chomsky3_node_literal(uint32_t codepoint) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_LITERAL);
    if (node) node->data.literal = codepoint;
    return node;
}

chomsky3_regex_node_t *chomsky3_node_char_class(chomsky3_cclass_flags_t flags) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_CHAR_CLASS);
    if (node) node->data.char_class.flags = flags;
    return node;
}

bool chomsky3_char_class_add_range(chomsky3_regex_node_t *node, uint32_t start, uint32_t end) {
    if (!node || node->type != CHOMSKY3_NODE_CHAR_CLASS) return false;
    size_t count = node->data.char_class.num_ranges;
    void *ranges = realloc(node->data.char_class.ranges, (count + 1) * sizeof(*node->data.char_class.ranges));
    if (!ranges) return false;
    node->data.char_class.ranges = ranges;
    node->data.char_class.ranges[count].start = start;
    node->data.char_class.ranges[count].end = end;
    node->data.char_class.num_ranges = count + 1;
    return true;
}

chomsky3_regex_node_t *chomsky3_node_concat(chomsky3_regex_node_t *left, chomsky3_regex_node_t *right) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_CONCAT);
    if (node) { node->left = left; node->right = right; }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_alternation(chomsky3_regex_node_t *left, chomsky3_regex_node_t *right) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_ALTERNATION);
    if (node) { node->left = left; node->right = right; }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_repeat(chomsky3_regex_node_t *child, uint32_t min, uint32_t max, bool greedy) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_REPEAT);
    if (node) {
        node->left = child;
        node->data.repeat.min = min;
        node->data.repeat.max = max;
        node->greedy = greedy;
    }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_group(chomsky3_regex_node_t *child, uint32_t group_id) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_GROUP);
    if (node) { node->left = child; node->data.group_id = group_id; }
    return node;
}

chomsky3_regex_node_t *chomsky3_node_backref(uint32_t backref_id) {
    chomsky3_regex_node_t *node = node_new(CHOMSKY3_NODE_BACKREF);
    if (node) node->data.backref_id = backref_id;
    return node;
}

chomsky3_regex_node_t *chomsky3_node_anchor(chomsky3_node_type_t type) { return node_new(type); }
chomsky3_regex_node_t *chomsky3_node_dot(void) { return node_new(CHOMSKY3_NODE_DOT); }

void chomsky3_node_free(chomsky3_regex_node_t *node) {
    if (!node) return;
    chomsky3_node_free(node->left);
    chomsky3_node_free(node->right);
    if (node->type == CHOMSKY3_NODE_CHAR_CLASS) free(node->data.char_class.ranges);
    free(node);
}

chomsky3_regex_node_t *chomsky3_node_clone(const chomsky3_regex_node_t *node) {
    if (!node) return NULL;
    chomsky3_regex_node_t *copy = node_new(node->type);
    if (!copy) return NULL;
    copy->data = node->data;
    copy->left = chomsky3_node_clone(node->left);
    copy->right = chomsky3_node_clone(node->right);
    copy->greedy = node->greedy;
    copy->position = node->position;
    if (node->type == CHOMSKY3_NODE_CHAR_CLASS && node->data.char_class.num_ranges) {
        size_t bytes = node->data.char_class.num_ranges * sizeof(*node->data.char_class.ranges);
        copy->data.char_class.ranges = malloc(bytes);
        if (copy->data.char_class.ranges) memcpy(copy->data.char_class.ranges, node->data.char_class.ranges, bytes);
    }
    return copy;
}

void chomsky3_node_print(const chomsky3_regex_node_t *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) fputs("  ", stdout);
    printf("node %d\n", (int)node->type);
}

size_t chomsky3_node_count(const chomsky3_regex_node_t *node) {
    return node ? 1 + chomsky3_node_count(node->left) + chomsky3_node_count(node->right) : 0;
}

bool chomsky3_regex_validate(const chomsky3_regex_t *regex) {
    return regex && regex->pattern;
}
