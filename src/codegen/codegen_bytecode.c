/**
 * libchomsky3 - Bytecode Code Generator
 *
 * Minimal block-IR aware bytecode scaffolding.
 */

#include "chomsky3/bytecode.h"
#include "chomsky3/ir.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CHOMSKY3_BYTECODE_MAGIC 0x43484F4D
#define CHOMSKY3_BYTECODE_VERSION_MAJOR 1
#define CHOMSKY3_BYTECODE_VERSION_MINOR 0
#define CHOMSKY3_BYTECODE_VERSION_PATCH 0

static chomsky3_bytecode_t *bytecode_alloc_stub(uint32_t flags) {
    chomsky3_bytecode_t *bytecode = calloc(1, sizeof(*bytecode));
    if (!bytecode) {
        return NULL;
    }

    bytecode->instructions = calloc(1, sizeof(*bytecode->instructions));
    if (!bytecode->instructions) {
        free(bytecode);
        return NULL;
    }

    bytecode->instructions[0].opcode = CHOMSKY3_OP_MATCH;
    bytecode->header.magic = CHOMSKY3_BYTECODE_MAGIC;
    bytecode->header.version.major = CHOMSKY3_BYTECODE_VERSION_MAJOR;
    bytecode->header.version.minor = CHOMSKY3_BYTECODE_VERSION_MINOR;
    bytecode->header.version.patch = CHOMSKY3_BYTECODE_VERSION_PATCH;
    bytecode->header.flags = flags;
    bytecode->header.num_instructions = 1;
    bytecode->code_size = sizeof(*bytecode->instructions);
    bytecode->total_size = sizeof(bytecode->header) + bytecode->code_size;
    return bytecode;
}

chomsky3_error_t chomsky3_bytecode_from_ir(
    chomsky3_context_t *ctx,
    const chomsky3_ir_t *ir,
    chomsky3_bytecode_t **bytecode
) {
    (void)ctx;
    if (!ir || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    *bytecode = bytecode_alloc_stub(ir->flags);
    return *bytecode ? CHOMSKY3_OK : CHOMSKY3_ERR_OUT_OF_MEMORY;
}

chomsky3_error_t chomsky3_bytecode_from_pattern(
    const chomsky3_pattern_t *pattern,
    chomsky3_bytecode_t **bytecode
) {
    if (!pattern || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    *bytecode = bytecode_alloc_stub(0);
    return *bytecode ? CHOMSKY3_OK : CHOMSKY3_ERR_OUT_OF_MEMORY;
}

void chomsky3_bytecode_free(chomsky3_bytecode_t *bytecode) {
    if (!bytecode) {
        return;
    }

    free(bytecode->instructions);
    free(bytecode->data);
    free(bytecode->pattern_source);
    free(bytecode->line_map);
    free(bytecode);
}

chomsky3_error_t chomsky3_bytecode_serialize(
    const chomsky3_bytecode_t *bytecode,
    uint8_t **buffer,
    size_t *size
) {
    if (!bytecode || !buffer || !size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    *size = sizeof(bytecode->header) + bytecode->code_size + bytecode->data_section_size;
    *buffer = malloc(*size);
    if (!*buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    uint8_t *ptr = *buffer;
    memcpy(ptr, &bytecode->header, sizeof(bytecode->header));
    ptr += sizeof(bytecode->header);
    if (bytecode->code_size) {
        memcpy(ptr, bytecode->instructions, bytecode->code_size);
        ptr += bytecode->code_size;
    }
    if (bytecode->data_section_size) {
        memcpy(ptr, bytecode->data, bytecode->data_section_size);
    }
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_bytecode_deserialize(
    chomsky3_context_t *ctx,
    const uint8_t *buffer,
    size_t size,
    chomsky3_bytecode_t **bytecode
) {
    (void)ctx;
    if (!buffer || size < sizeof(chomsky3_bytecode_header_t) || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    chomsky3_bytecode_header_t header;
    memcpy(&header, buffer, sizeof(header));
    if (header.magic != CHOMSKY3_BYTECODE_MAGIC) {
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    *bytecode = bytecode_alloc_stub(header.flags);
    if (!*bytecode) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    (*bytecode)->header = header;
    return CHOMSKY3_OK;
}

chomsky3_error_t chomsky3_bytecode_save(
    const chomsky3_bytecode_t *bytecode,
    const char *path
) {
    if (!bytecode || !path) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    uint8_t *buffer = NULL;
    size_t size = 0;
    chomsky3_error_t err = chomsky3_bytecode_serialize(bytecode, &buffer, &size);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        free(buffer);
        return CHOMSKY3_ERR_IO_ERROR;
    }
    size_t written = fwrite(buffer, 1, size, file);
    fclose(file);
    free(buffer);
    return written == size ? CHOMSKY3_OK : CHOMSKY3_ERR_IO_ERROR;
}

chomsky3_error_t chomsky3_bytecode_load(
    chomsky3_context_t *ctx,
    const char *path,
    chomsky3_bytecode_t **bytecode
) {
    if (!path || !bytecode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return CHOMSKY3_ERR_IO_ERROR;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return CHOMSKY3_ERR_IO_ERROR;
    }
    long file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return CHOMSKY3_ERR_IO_ERROR;
    }

    uint8_t *buffer = malloc((size_t)file_size);
    if (!buffer) {
        fclose(file);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    size_t read_size = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);
    if (read_size != (size_t)file_size) {
        free(buffer);
        return CHOMSKY3_ERR_IO_ERROR;
    }

    chomsky3_error_t err = chomsky3_bytecode_deserialize(ctx, buffer, read_size, bytecode);
    free(buffer);
    return err;
}

bool chomsky3_bytecode_validate(const chomsky3_bytecode_t *bytecode) {
    return bytecode && bytecode->header.magic == CHOMSKY3_BYTECODE_MAGIC &&
           bytecode->instructions && bytecode->header.num_instructions > 0;
}

chomsky3_bytecode_version_t chomsky3_bytecode_get_version(
    const chomsky3_bytecode_t *bytecode
) {
    return bytecode ? bytecode->header.version : (chomsky3_bytecode_version_t){0, 0, 0};
}

chomsky3_error_t chomsky3_bytecode_disassemble(
    const chomsky3_bytecode_t *bytecode,
    char **output
) {
    if (!bytecode || !output) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    int needed = snprintf(NULL, 0, "bytecode: %u instructions\n", bytecode->header.num_instructions);
    if (needed < 0) {
        return CHOMSKY3_ERR_INTERNAL;
    }
    *output = malloc((size_t)needed + 1);
    if (!*output) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    snprintf(*output, (size_t)needed + 1, "bytecode: %u instructions\n", bytecode->header.num_instructions);
    return CHOMSKY3_OK;
}

const char *chomsky3_opcode_name(chomsky3_opcode_t opcode) {
    switch (opcode) {
        case CHOMSKY3_OP_MATCH: return "MATCH";
        case CHOMSKY3_OP_FAIL: return "FAIL";
        case CHOMSKY3_OP_CHAR: return "CHAR";
        case CHOMSKY3_OP_STRING: return "STRING";
        case CHOMSKY3_OP_NOP: return "NOP";
        default: return "UNKNOWN";
    }
}

chomsky3_error_t chomsky3_bytecode_optimize(
    const chomsky3_bytecode_t *bytecode,
    int level,
    chomsky3_bytecode_t **optimized
) {
    (void)level;
    if (!bytecode || !optimized) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    uint8_t *buffer = NULL;
    size_t size = 0;
    chomsky3_error_t err = chomsky3_bytecode_serialize(bytecode, &buffer, &size);
    if (err != CHOMSKY3_OK) {
        return err;
    }
    err = chomsky3_bytecode_deserialize(NULL, buffer, size, optimized);
    free(buffer);
    return err;
}

bool chomsky3_bytecode_verify(
    const chomsky3_bytecode_t *bytecode,
    char **errors
) {
    bool valid = chomsky3_bytecode_validate(bytecode);
    if (!valid && errors) {
        const char *message = "invalid bytecode";
        *errors = malloc(strlen(message) + 1);
        if (*errors) {
            strcpy(*errors, message);
        }
    }
    return valid;
}

uint32_t chomsky3_bytecode_complexity(const chomsky3_bytecode_t *bytecode) {
    return bytecode ? bytecode->header.num_instructions : 0;
}
