/**
 * libchomsky3 - Binary Code Serialization
 * 
 * Advanced serialization utilities for binary code, including compression,
 * relocation tables, symbol tables, and debug information.
 */

#include "chomsky3/bindcode.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Serialization format constants */
#define CHOMSKY3_SECTION_CODE       0x01
#define CHOMSKY3_SECTION_RELOC      0x02
#define CHOMSKY3_SECTION_SYMBOLS    0x03
#define CHOMSKY3_SECTION_DEBUG      0x04
#define CHOMSKY3_SECTION_METADATA   0x05
#define CHOMSKY3_SECTION_END        0xFF

/* Compression methods */
#define CHOMSKY3_COMPRESS_NONE      0x00
#define CHOMSKY3_COMPRESS_RLE       0x01
#define CHOMSKY3_COMPRESS_LZ4       0x02

/* Section header */
typedef struct {
    uint8_t type;               /* Section type */
    uint8_t flags;              /* Section flags */
    uint8_t compression;        /* Compression method */
    uint8_t reserved;           /* Reserved */
    uint32_t uncompressed_size; /* Original size */
    uint32_t compressed_size;   /* Compressed size */
    uint32_t offset;            /* Offset in file */
} chomsky3_section_header_t;

/* Relocation entry */
typedef struct {
    uint32_t offset;            /* Offset in code */
    uint32_t type;              /* Relocation type */
    uint32_t symbol_index;      /* Symbol table index */
    int32_t addend;             /* Addend value */
} chomsky3_reloc_entry_t;

/* Symbol table entry */
typedef struct {
    uint32_t name_offset;       /* Offset to name string */
    uint32_t value;             /* Symbol value/address */
    uint32_t size;              /* Symbol size */
    uint8_t type;               /* Symbol type */
    uint8_t binding;            /* Symbol binding */
    uint16_t section;           /* Section index */
} chomsky3_symbol_entry_t;

/* Debug line info entry */
typedef struct {
    uint32_t code_offset;       /* Offset in code */
    uint32_t line;              /* Source line number */
    uint32_t column;            /* Source column */
    uint32_t file_index;        /* Source file index */
} chomsky3_debug_line_t;

/* Serialization context */
typedef struct {
    uint8_t *buffer;            /* Output buffer */
    size_t capacity;            /* Buffer capacity */
    size_t position;            /* Current write position */
    chomsky3_error_t error;     /* Last error */
} chomsky3_serialize_ctx_t;

/* Forward declarations */
static chomsky3_error_t write_section_header(
    chomsky3_serialize_ctx_t *ctx,
    const chomsky3_section_header_t *header
);
static chomsky3_error_t write_bytes(
    chomsky3_serialize_ctx_t *ctx,
    const void *data,
    size_t size
);
static chomsky3_error_t write_uint32(
    chomsky3_serialize_ctx_t *ctx,
    uint32_t value
);
static chomsky3_error_t write_uint64(
    chomsky3_serialize_ctx_t *ctx,
    uint64_t value
);
static chomsky3_error_t ensure_capacity(
    chomsky3_serialize_ctx_t *ctx,
    size_t required
);
static chomsky3_error_t compress_rle(
    const uint8_t *input,
    size_t input_size,
    uint8_t **output,
    size_t *output_size
);
static chomsky3_error_t decompress_rle(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_size
);

/**
 * Initialize serialization context.
 */
static chomsky3_error_t init_serialize_ctx(
    chomsky3_serialize_ctx_t *ctx,
    size_t initial_capacity
) {
    ctx->buffer = malloc(initial_capacity);
    if (!ctx->buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    
    ctx->capacity = initial_capacity;
    ctx->position = 0;
    ctx->error = CHOMSKY3_OK;
    
    return CHOMSKY3_OK;
}

/**
 * Free serialization context.
 */
static void free_serialize_ctx(chomsky3_serialize_ctx_t *ctx) {
    free(ctx->buffer);
    ctx->buffer = NULL;
    ctx->capacity = 0;
    ctx->position = 0;
}

/**
 * Serialize binary code with advanced features.
 */
chomsky3_error_t chomsky3_bincode_serialize_advanced(
    const chomsky3_bincode_t *bincode,
    uint32_t flags,
    uint8_t **out_buffer,
    size_t *out_size
) {
    if (!bincode || !out_buffer || !out_size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    chomsky3_serialize_ctx_t ctx;
    chomsky3_error_t err = init_serialize_ctx(&ctx, bincode->code_size * 2);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    /* Write code section */
    chomsky3_section_header_t code_header = {
        .type = CHOMSKY3_SECTION_CODE,
        .flags = 0,
        .compression = (flags & CHOMSKY3_SERIALIZE_COMPRESS) ? 
                       CHOMSKY3_COMPRESS_RLE : CHOMSKY3_COMPRESS_NONE,
        .reserved = 0,
        .uncompressed_size = bincode->code_size,
        .compressed_size = 0,
        .offset = 0
    };

    uint8_t *code_data = NULL;
    size_t code_size = 0;

    if (code_header.compression == CHOMSKY3_COMPRESS_RLE) {
        err = compress_rle(bincode->code, bincode->code_size, 
                          &code_data, &code_size);
        if (err != CHOMSKY3_OK) {
            free_serialize_ctx(&ctx);
            return err;
        }
        code_header.compressed_size = code_size;
    } else {
        code_data = bincode->code;
        code_size = bincode->code_size;
        code_header.compressed_size = code_size;
    }

    code_header.offset = ctx.position + sizeof(chomsky3_section_header_t);
    
    err = write_section_header(&ctx, &code_header);
    if (err != CHOMSKY3_OK) {
        if (code_header.compression != CHOMSKY3_COMPRESS_NONE) {
            free(code_data);
        }
        free_serialize_ctx(&ctx);
        return err;
    }

    err = write_bytes(&ctx, code_data, code_size);
    if (code_header.compression != CHOMSKY3_COMPRESS_NONE) {
        free(code_data);
    }
    if (err != CHOMSKY3_OK) {
        free_serialize_ctx(&ctx);
        return err;
    }

    /* Write metadata section if present */
    if (bincode->metadata && (flags & CHOMSKY3_SERIALIZE_METADATA)) {
        size_t meta_len = strlen(bincode->metadata);
        
        chomsky3_section_header_t meta_header = {
            .type = CHOMSKY3_SECTION_METADATA,
            .flags = 0,
            .compression = CHOMSKY3_COMPRESS_NONE,
            .reserved = 0,
            .uncompressed_size = meta_len,
            .compressed_size = meta_len,
            .offset = ctx.position + sizeof(chomsky3_section_header_t)
        };

        err = write_section_header(&ctx, &meta_header);
        if (err != CHOMSKY3_OK) {
            free_serialize_ctx(&ctx);
            return err;
        }

        err = write_bytes(&ctx, bincode->metadata, meta_len);
        if (err != CHOMSKY3_OK) {
            free_serialize_ctx(&ctx);
            return err;
        }
    }

    /* Write end marker */
    chomsky3_section_header_t end_header = {
        .type = CHOMSKY3_SECTION_END,
        .flags = 0,
        .compression = 0,
        .reserved = 0,
        .uncompressed_size = 0,
        .compressed_size = 0,
        .offset = 0
    };

    err = write_section_header(&ctx, &end_header);
    if (err != CHOMSKY3_OK) {
        free_serialize_ctx(&ctx);
        return err;
    }

    /* Return buffer */
    *out_buffer = ctx.buffer;
    *out_size = ctx.position;

    return CHOMSKY3_OK;
}

/**
 * Deserialize binary code with advanced features.
 */
chomsky3_error_t chomsky3_bincode_deserialize_advanced(
    const uint8_t *buffer,
    size_t size,
    chomsky3_bincode_t **out_bincode
) {
    if (!buffer || size == 0 || !out_bincode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    const uint8_t *ptr = buffer;
    const uint8_t *end = buffer + size;

    uint8_t *code_data = NULL;
    size_t code_size = 0;
    chomsky3_arch_t arch = CHOMSKY3_ARCH_X86_64;  /* Default */
    char *metadata = NULL;

    /* Read sections */
    while (ptr < end) {
        if (ptr + sizeof(chomsky3_section_header_t) > end) {
            free(code_data);
            free(metadata);
            return CHOMSKY3_ERR_INVALID_FORMAT;
        }

        const chomsky3_section_header_t *header = 
            (const chomsky3_section_header_t *)ptr;
        ptr += sizeof(chomsky3_section_header_t);

        if (header->type == CHOMSKY3_SECTION_END) {
            break;
        }

        if (ptr + header->compressed_size > end) {
            free(code_data);
            free(metadata);
            return CHOMSKY3_ERR_INVALID_FORMAT;
        }

        switch (header->type) {
            case CHOMSKY3_SECTION_CODE: {
                if (header->compression == CHOMSKY3_COMPRESS_NONE) {
                    code_size = header->compressed_size;
                    code_data = malloc(code_size);
                    if (!code_data) {
                        free(metadata);
                        return CHOMSKY3_ERR_OUT_OF_MEMORY;
                    }
                    memcpy(code_data, ptr, code_size);
                } else if (header->compression == CHOMSKY3_COMPRESS_RLE) {
                    code_size = header->uncompressed_size;
                    code_data = malloc(code_size);
                    if (!code_data) {
                        free(metadata);
                        return CHOMSKY3_ERR_OUT_OF_MEMORY;
                    }
                    chomsky3_error_t err = decompress_rle(
                        ptr, header->compressed_size,
                        code_data, code_size
                    );
                    if (err != CHOMSKY3_OK) {
                        free(code_data);
                        free(metadata);
                        return err;
                    }
                } else {
                    free(code_data);
                    free(metadata);
                    return CHOMSKY3_ERR_UNSUPPORTED_COMPRESSION;
                }
                break;
            }

            case CHOMSKY3_SECTION_METADATA: {
                metadata = malloc(header->compressed_size + 1);
                if (!metadata) {
                    free(code_data);
                    return CHOMSKY3_ERR_OUT_OF_MEMORY;
                }
                memcpy(metadata, ptr, header->compressed_size);
                metadata[header->compressed_size] = '\0';
                break;
            }

            case CHOMSKY3_SECTION_RELOC:
            case CHOMSKY3_SECTION_SYMBOLS:
            case CHOMSKY3_SECTION_DEBUG:
                /* Skip unsupported sections for now */
                break;

            default:
                /* Unknown section, skip */
                break;
        }

        ptr += header->compressed_size;
    }

    /* Validate we got code */
    if (!code_data) {
        free(metadata);
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_FORMAT,
                          "No code section found");
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    /* Create bincode object */
    chomsky3_bincode_t *bincode = chomsky3_bincode_create(
        arch, code_data, code_size
    );
    free(code_data);

    if (!bincode) {
        free(metadata);
        return chomsky3_get_last_error();
    }

    if (metadata) {
        bincode->metadata = metadata;
    }

    *out_bincode = bincode;
    return CHOMSKY3_OK;
}

/**
 * Serialize relocation table.
 */
chomsky3_error_t chomsky3_bincode_serialize_relocations(
    const chomsky3_reloc_entry_t *relocs,
    size_t count,
    uint8_t **out_buffer,
    size_t *out_size
) {
    if (!relocs || count == 0 || !out_buffer || !out_size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    size_t size = sizeof(uint32_t) + count * sizeof(chomsky3_reloc_entry_t);
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    uint8_t *ptr = buffer;

    /* Write count */
    *(uint32_t *)ptr = count;
    ptr += sizeof(uint32_t);

    /* Write entries */
    memcpy(ptr, relocs, count * sizeof(chomsky3_reloc_entry_t));

    *out_buffer = buffer;
    *out_size = size;

    return CHOMSKY3_OK;
}

/**
 * Deserialize relocation table.
 */
chomsky3_error_t chomsky3_bincode_deserialize_relocations(
    const uint8_t *buffer,
    size_t size,
    chomsky3_reloc_entry_t **out_relocs,
    size_t *out_count
) {
    if (!buffer || size < sizeof(uint32_t) || !out_relocs || !out_count) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    const uint8_t *ptr = buffer;

    /* Read count */
    uint32_t count = *(const uint32_t *)ptr;
    ptr += sizeof(uint32_t);

    /* Validate size */
    size_t expected_size = sizeof(uint32_t) + 
                          count * sizeof(chomsky3_reloc_entry_t);
    if (size < expected_size) {
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    /* Allocate and copy entries */
    chomsky3_reloc_entry_t *relocs = malloc(
        count * sizeof(chomsky3_reloc_entry_t)
    );
    if (!relocs) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    memcpy(relocs, ptr, count * sizeof(chomsky3_reloc_entry_t));

    *out_relocs = relocs;
    *out_count = count;

    return CHOMSKY3_OK;
}

/**
 * Serialize symbol table.
 */
chomsky3_error_t chomsky3_bincode_serialize_symbols(
    const chomsky3_symbol_entry_t *symbols,
    size_t count,
    const char **names,
    uint8_t **out_buffer,
    size_t *out_size
) {
    if (!symbols || count == 0 || !names || !out_buffer || !out_size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Calculate string table size */
    size_t string_table_size = 0;
    for (size_t i = 0; i < count; i++) {
        if (names[i]) {
            string_table_size += strlen(names[i]) + 1;
        }
    }

    /* Calculate total size */
    size_t size = sizeof(uint32_t) +  /* count */
                  count * sizeof(chomsky3_symbol_entry_t) +
                  sizeof(uint32_t) +  /* string table size */
                  string_table_size;

    uint8_t *buffer = malloc(size);
    if (!buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    uint8_t *ptr = buffer;

    /* Write count */
    *(uint32_t *)ptr = count;
    ptr += sizeof(uint32_t);

    /* Write symbol entries */
    memcpy(ptr, symbols, count * sizeof(chomsky3_symbol_entry_t));
    ptr += count * sizeof(chomsky3_symbol_entry_t);

    /* Write string table size */
    *(uint32_t *)ptr = string_table_size;
    ptr += sizeof(uint32_t);

    /* Write string table */
    for (size_t i = 0; i < count; i++) {
        if (names[i]) {
            size_t len = strlen(names[i]) + 1;
            memcpy(ptr, names[i], len);
            ptr += len;
        }
    }

    *out_buffer = buffer;
    *out_size = size;

    return CHOMSKY3_OK;
}

/**
 * Serialize debug information.
 */
chomsky3_error_t chomsky3_bincode_serialize_debug_info(
    const chomsky3_debug_line_t *lines,
    size_t count,
    const char **files,
    size_t file_count,
    uint8_t **out_buffer,
    size_t *out_size
) {
    if (!lines || count == 0 || !out_buffer || !out_size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Calculate file table size */
    size_t file_table_size = 0;
    if (files) {
        for (size_t i = 0; i < file_count; i++) {
            if (files[i]) {
                file_table_size += strlen(files[i]) + 1;
            }
        }
    }

    /* Calculate total size */
    size_t size = sizeof(uint32_t) +  /* line count */
                  count * sizeof(chomsky3_debug_line_t) +
                  sizeof(uint32_t) +  /* file count */
                  file_table_size;

    uint8_t *buffer = malloc(size);
    if (!buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    uint8_t *ptr = buffer;

    /* Write line count */
    *(uint32_t *)ptr = count;
    ptr += sizeof(uint32_t);

    /* Write line entries */
    memcpy(ptr, lines, count * sizeof(chomsky3_debug_line_t));
    ptr += count * sizeof(chomsky3_debug_line_t);

    /* Write file count */
    *(uint32_t *)ptr = file_count;
    ptr += sizeof(uint32_t);

    /* Write file table */
    if (files) {
        for (size_t i = 0; i < file_count; i++) {
            if (files[i]) {
                size_t len = strlen(files[i]) + 1;
                memcpy(ptr, files[i], len);
                ptr += len;
            }
        }
    }

    *out_buffer = buffer;
    *out_size = size;

    return CHOMSKY3_OK;
}

/**
 * Write section header to context.
 */
static chomsky3_error_t write_section_header(
    chomsky3_serialize_ctx_t *ctx,
    const chomsky3_section_header_t *header
) {
    return write_bytes(ctx, header, sizeof(chomsky3_section_header_t));
}

/**
 * Write bytes to context.
 */
static chomsky3_error_t write_bytes(
    chomsky3_serialize_ctx_t *ctx,
    const void *data,
    size_t size
) {
    chomsky3_error_t err = ensure_capacity(ctx, ctx->position + size);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    memcpy(ctx->buffer + ctx->position, data, size);
    ctx->position += size;

    return CHOMSKY3_OK;
}

/**
 * Write uint32 to context.
 */
static chomsky3_error_t write_uint32(
    chomsky3_serialize_ctx_t *ctx,
    uint32_t value
) {
    return write_bytes(ctx, &value, sizeof(uint32_t));
}

/**
 * Write uint64 to context.
 */
static chomsky3_error_t write_uint64(
    chomsky3_serialize_ctx_t *ctx,
    uint64_t value
) {
    return write_bytes(ctx, &value, sizeof(uint64_t));
}

/**
 * Ensure context has required capacity.
 */
static chomsky3_error_t ensure_capacity(
    chomsky3_serialize_ctx_t *ctx,
    size_t required
) {
    if (required <= ctx->capacity) {
        return CHOMSKY3_OK;
    }

    size_t new_capacity = ctx->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    uint8_t *new_buffer = realloc(ctx->buffer, new_capacity);
    if (!new_buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    ctx->buffer = new_buffer;
    ctx->capacity = new_capacity;

    return CHOMSKY3_OK;
}

/**
 * Simple RLE compression.
 */
static chomsky3_error_t compress_rle(
    const uint8_t *input,
    size_t input_size,
    uint8_t **output,
    size_t *output_size
) {
    if (!input || input_size == 0 || !output || !output_size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Worst case: every byte is different */
    size_t max_size = input_size * 2;
    uint8_t *buffer = malloc(max_size);
    if (!buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    size_t out_pos = 0;
    size_t i = 0;

    while (i < input_size) {
        uint8_t byte = input[i];
        size_t run_length = 1;

        /* Count run */
        while (i + run_length < input_size && 
               input[i + run_length] == byte && 
               run_length < 255) {
            run_length++;
        }

        /* Write run */
        if (run_length >= 3 || byte == 0xFF) {
            /* RLE encoded */
            buffer[out_pos++] = 0xFF;  /* Escape byte */
            buffer[out_pos++] = run_length;
            buffer[out_pos++] = byte;
        } else {
            /* Literal bytes */
            for (size_t j = 0; j < run_length; j++) {
                buffer[out_pos++] = byte;
            }
        }

        i += run_length;
    }

    *output = buffer;
    *output_size = out_pos;

    return CHOMSKY3_OK;
}

/**
 * Simple RLE decompression.
 */
static chomsky3_error_t decompress_rle(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_size
) {
    if (!input || !output || input_size == 0 || output_size == 0) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_size && out_pos < output_size) {
        if (input[in_pos] == 0xFF && in_pos + 2 < input_size) {
            /* RLE encoded run */
            size_t run_length = input[in_pos + 1];
            uint8_t byte = input[in_pos + 2];
            in_pos += 3;

            if (out_pos + run_length > output_size) {
                return CHOMSKY3_ERR_BUFFER_TOO_SMALL;
            }

            memset(output + out_pos, byte, run_length);
            out_pos += run_length;
        } else {
            /* Literal byte */
            output[out_pos++] = input[in_pos++];
        }
    }

    if (out_pos != output_size) {
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    return CHOMSKY3_OK;
}

/**
 * Calculate serialized size estimate.
 */
size_t chomsky3_bincode_estimate_size(
    const chomsky3_bincode_t *bincode,
    uint32_t flags
) {
    if (!bincode) {
        return 0;
    }

    size_t size = sizeof(chomsky3_section_header_t) + bincode->code_size;

    if (flags & CHOMSKY3_SERIALIZE_METADATA && bincode->metadata) {
        size += sizeof(chomsky3_section_header_t) + strlen(bincode->metadata);
    }

    size += sizeof(chomsky3_section_header_t);  /* End marker */

    /* Add overhead for compression */
    if (flags & CHOMSKY3_SERIALIZE_COMPRESS) {
        size = (size * 3) / 2;  /* 50% overhead estimate */
    }

    return size;
}