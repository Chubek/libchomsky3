/**
 * libchomsky3 - Binary Code Format Implementation
 * 
 * Implementation of binary code format handling, serialization, and metadata
 * management for JIT-compiled native code.
 */

#define _POSIX_C_SOURCE 200809L
#include "chomsky3/bincode.h"
#include "chomsky3/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

/* Helper to set error with message */
static void chomsky3_set_error(chomsky3_error_t code, const char *fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    chomsky3_error_info_t info = {
        .code = code,
        .severity = CHOMSKY3_SEVERITY_ERROR,
        .message = buffer,
        .location = {0},
        .context = NULL,
        .suggestion = NULL,
        .user_data = NULL
    };
    chomsky3_set_last_error(&info);
}

/* Magic number for binary code format */
#define CHOMSKY3_BINCODE_MAGIC 0x43484F4D  /* "CHOM" */

/* Current format version */
#define CHOMSKY3_BINCODE_VERSION 1

/* Binary code file header */
typedef struct {
    uint32_t magic;             /* Magic number */
    uint32_t version;           /* Format version */
    uint32_t arch;              /* Target architecture */
    uint32_t flags;             /* Compilation flags */
    uint64_t timestamp;         /* Creation timestamp */
    uint32_t code_size;         /* Size of native code */
    uint32_t metadata_size;     /* Size of metadata section */
    uint32_t checksum;          /* CRC32 checksum */
    uint32_t reserved[8];       /* Reserved for future use */
} chomsky3_bincode_header_t;

/* Internal metadata structure */
typedef struct {
    char *pattern;              /* Original regex pattern */
    uint32_t pattern_length;    /* Pattern length */
    uint32_t num_groups;        /* Number of capturing groups */
    uint32_t opt_level;         /* Optimization level used */
    uint64_t compile_time_us;   /* Compilation time in microseconds */
} chomsky3_bincode_metadata_internal_t;

/* Forward declarations */
static uint32_t compute_crc32(const uint8_t *data, size_t length);
static chomsky3_error_t serialize_metadata(
    const chomsky3_bincode_metadata_internal_t *metadata,
    uint8_t **out_data,
    size_t *out_size
);
static chomsky3_error_t deserialize_metadata(
    const uint8_t *data,
    size_t size,
    chomsky3_bincode_metadata_internal_t *metadata
);

/**
 * Create a new binary code structure.
 */
chomsky3_bincode_t *chomsky3_bincode_create(
    chomsky3_arch_t arch,
    const void *code,
    size_t code_size
) {
    if (!code || code_size == 0) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_ARGUMENT,
                          "Invalid code or size");
        return NULL;
    }

    chomsky3_bincode_t *bincode = calloc(1, sizeof(chomsky3_bincode_t));
    if (!bincode) {
        chomsky3_set_error(CHOMSKY3_ERR_OUT_OF_MEMORY,
                          "Failed to allocate bincode structure");
        return NULL;
    }

    /* Allocate and copy code */
    bincode->code = malloc(code_size);
    if (!bincode->code) {
        free(bincode);
        chomsky3_set_error(CHOMSKY3_ERR_OUT_OF_MEMORY,
                          "Failed to allocate code buffer");
        return NULL;
    }

    memcpy(bincode->code, code, code_size);
    bincode->code_size = code_size;
    bincode->arch = arch;
    bincode->entry_point = bincode->code;  /* Default entry point */

    return bincode;
}

/**
 * Free a binary code structure.
 */
void chomsky3_bincode_free(chomsky3_bincode_t *bincode) {
    if (!bincode) {
        return;
    }

    free(bincode->code);
    free(bincode->metadata);
    free(bincode);
}

/**
 * Serialize binary code to a buffer.
 */
chomsky3_error_t chomsky3_bincode_serialize(
    const chomsky3_bincode_t *bincode,
    uint8_t **out_buffer,
    size_t *out_size
) {
    if (!bincode || !out_buffer || !out_size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Prepare metadata */
    uint8_t *metadata_data = NULL;
    size_t metadata_size = 0;

    if (bincode->metadata) {
        chomsky3_bincode_metadata_internal_t internal_meta = {
            .pattern = bincode->metadata,
            .pattern_length = bincode->metadata ? strlen(bincode->metadata) : 0,
            .num_groups = 0,  /* TODO: Extract from bincode if available */
            .opt_level = 0,
            .compile_time_us = 0
        };

        chomsky3_error_t err = serialize_metadata(&internal_meta, 
                                                   &metadata_data, 
                                                   &metadata_size);
        if (err != CHOMSKY3_OK) {
            return err;
        }
    }

    /* Calculate total size */
    size_t total_size = sizeof(chomsky3_bincode_header_t) + 
                        bincode->code_size + 
                        metadata_size;

    /* Allocate output buffer */
    uint8_t *buffer = malloc(total_size);
    if (!buffer) {
        free(metadata_data);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    /* Fill header */
    chomsky3_bincode_header_t *header = (chomsky3_bincode_header_t *)buffer;
    header->magic = CHOMSKY3_BINCODE_MAGIC;
    header->version = CHOMSKY3_BINCODE_VERSION;
    header->arch = bincode->arch;
    header->flags = 0;  /* TODO: Store compilation flags */
    header->timestamp = (uint64_t)time(NULL);
    header->code_size = bincode->code_size;
    header->metadata_size = metadata_size;
    memset(header->reserved, 0, sizeof(header->reserved));

    /* Copy code */
    uint8_t *code_ptr = buffer + sizeof(chomsky3_bincode_header_t);
    memcpy(code_ptr, bincode->code, bincode->code_size);

    /* Copy metadata */
    if (metadata_size > 0) {
        uint8_t *meta_ptr = code_ptr + bincode->code_size;
        memcpy(meta_ptr, metadata_data, metadata_size);
        free(metadata_data);
    }

    /* Compute checksum (excluding checksum field itself) */
    header->checksum = 0;
    header->checksum = compute_crc32(buffer, total_size);

    *out_buffer = buffer;
    *out_size = total_size;

    return CHOMSKY3_OK;
}

/**
 * Deserialize binary code from a buffer.
 */
chomsky3_error_t chomsky3_bincode_deserialize(
    const uint8_t *buffer,
    size_t size,
    chomsky3_bincode_t **out_bincode
) {
    if (!buffer || size < sizeof(chomsky3_bincode_header_t) || !out_bincode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Read and validate header */
    const chomsky3_bincode_header_t *header = 
        (const chomsky3_bincode_header_t *)buffer;

    if (header->magic != CHOMSKY3_BINCODE_MAGIC) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_FORMAT,
                          "Invalid magic number in binary code");
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    if (header->version != CHOMSKY3_BINCODE_VERSION) {
        chomsky3_set_error(CHOMSKY3_ERR_UNSUPPORTED_VERSION,
                          "Unsupported binary code version: %u", 
                          header->version);
        return CHOMSKY3_ERR_UNSUPPORTED_VERSION;
    }

    /* Verify size */
    size_t expected_size = sizeof(chomsky3_bincode_header_t) + 
                           header->code_size + 
                           header->metadata_size;
    if (size < expected_size) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_FORMAT,
                          "Buffer too small for binary code");
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    /* Verify checksum */
    uint32_t stored_checksum = header->checksum;
    uint8_t *temp_buffer = malloc(size);
    if (!temp_buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }
    memcpy(temp_buffer, buffer, size);
    ((chomsky3_bincode_header_t *)temp_buffer)->checksum = 0;
    uint32_t computed_checksum = compute_crc32(temp_buffer, size);
    free(temp_buffer);

    if (stored_checksum != computed_checksum) {
        chomsky3_set_error(CHOMSKY3_ERR_CHECKSUM_MISMATCH,
                          "Checksum mismatch in binary code");
        return CHOMSKY3_ERR_CHECKSUM_MISMATCH;
    }

    /* Extract code */
    const uint8_t *code_ptr = buffer + sizeof(chomsky3_bincode_header_t);
    
    chomsky3_bincode_t *bincode = chomsky3_bincode_create(
        (chomsky3_arch_t)header->arch,
        code_ptr,
        header->code_size
    );

    if (!bincode) {
        const chomsky3_error_info_t *err = chomsky3_get_last_error();
        return err ? err->code : CHOMSKY3_ERROR_GENERIC;
    }

    /* Extract metadata if present */
    if (header->metadata_size > 0) {
        const uint8_t *meta_ptr = code_ptr + header->code_size;
        chomsky3_bincode_metadata_internal_t internal_meta;
        
        chomsky3_error_t err = deserialize_metadata(meta_ptr, 
                                                     header->metadata_size,
                                                     &internal_meta);
        if (err == CHOMSKY3_OK && internal_meta.pattern) {
            bincode->metadata = strdup(internal_meta.pattern);
            free(internal_meta.pattern);
        }
    }

    *out_bincode = bincode;
    return CHOMSKY3_OK;
}

/**
 * Save binary code to a file.
 */
chomsky3_error_t chomsky3_bincode_save(
    const chomsky3_bincode_t *bincode,
    const char *filename
) {
    if (!bincode || !filename) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Serialize to buffer */
    uint8_t *buffer = NULL;
    size_t size = 0;
    chomsky3_error_t err = chomsky3_bincode_serialize(bincode, &buffer, &size);
    if (err != CHOMSKY3_OK) {
        return err;
    }

    /* Write to file */
    FILE *file = fopen(filename, "wb");
    if (!file) {
        free(buffer);
        chomsky3_set_error(CHOMSKY3_ERR_IO_ERROR,
                          "Failed to open file for writing: %s", filename);
        return CHOMSKY3_ERR_IO_ERROR;
    }

    size_t written = fwrite(buffer, 1, size, file);
    fclose(file);
    free(buffer);

    if (written != size) {
        chomsky3_set_error(CHOMSKY3_ERR_IO_ERROR,
                          "Failed to write complete binary code to file");
        return CHOMSKY3_ERR_IO_ERROR;
    }

    return CHOMSKY3_OK;
}

/**
 * Load binary code from a file.
 */
chomsky3_error_t chomsky3_bincode_load(
    chomsky3_context_t *ctx,
    const char *filename,
    chomsky3_bincode_t **out_bincode
) {
    (void)ctx;  /* Unused for now */
    if (!filename || !out_bincode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Open file */
    FILE *file = fopen(filename, "rb");
    if (!file) {
        chomsky3_set_error(CHOMSKY3_ERR_IO_ERROR,
                          "Failed to open file for reading: %s", filename);
        return CHOMSKY3_ERR_IO_ERROR;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        chomsky3_set_error(CHOMSKY3_ERR_IO_ERROR,
                          "Invalid file size");
        return CHOMSKY3_ERR_IO_ERROR;
    }

    /* Read file */
    uint8_t *buffer = malloc(file_size);
    if (!buffer) {
        fclose(file);
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    size_t read = fread(buffer, 1, file_size, file);
    fclose(file);

    if (read != (size_t)file_size) {
        free(buffer);
        chomsky3_set_error(CHOMSKY3_ERR_IO_ERROR,
                          "Failed to read complete file");
        return CHOMSKY3_ERR_IO_ERROR;
    }

    /* Deserialize */
    chomsky3_error_t err = chomsky3_bincode_deserialize(buffer, 
                                                         file_size, 
                                                         out_bincode);
    free(buffer);

    return err;
}

/**
 * Verify binary code integrity.
 */
chomsky3_error_t chomsky3_bincode_verify(const chomsky3_bincode_t *bincode) {
    if (!bincode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Basic validation */
    if (!bincode->code || bincode->code_size == 0) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_FORMAT,
                          "Binary code has no code section");
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    if (!bincode->entry_point) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_FORMAT,
                          "Binary code has no entry point");
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    /* Verify entry point is within code bounds */
    const uint8_t *code_start = bincode->code;
    const uint8_t *code_end = code_start + bincode->code_size;
    const uint8_t *entry = bincode->entry_point;

    if (entry < code_start || entry >= code_end) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_FORMAT,
                          "Entry point outside code bounds");
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    /* Architecture-specific validation could go here */

    return CHOMSKY3_OK;
}

/**
 * Get binary code information.
 */
chomsky3_error_t chomsky3_bincode_get_info(
    const chomsky3_bincode_t *bincode,
    chomsky3_arch_t *arch,
    size_t *code_size,
    const char **metadata
) {
    if (!bincode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    if (arch) {
        *arch = bincode->arch;
    }

    if (code_size) {
        *code_size = bincode->code_size;
    }

    if (metadata) {
        *metadata = bincode->metadata;
    }

    return CHOMSKY3_OK;
}

/**
 * Clone binary code.
 */
chomsky3_error_t chomsky3_bincode_clone(
    const chomsky3_bincode_t *bincode,
    chomsky3_bincode_t **cloned
) {
    if (!cloned) return CHOMSKY3_ERR_INVALID_ARGUMENT;
    if (!bincode) {
        chomsky3_set_error(CHOMSKY3_ERR_INVALID_ARGUMENT,
                          "Cannot clone NULL bincode");
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    chomsky3_bincode_t *clone = chomsky3_bincode_create(
        bincode->arch,
        bincode->code,
        bincode->code_size
    );

    if (!clone) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    /* Copy metadata */
    if (bincode->metadata) {
        clone->metadata = strdup(bincode->metadata);
        if (!clone->metadata) {
            chomsky3_bincode_free(clone);
            chomsky3_set_error(CHOMSKY3_ERR_OUT_OF_MEMORY,
                              "Failed to copy metadata");
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
    }

    /* Copy entry point offset */
    size_t entry_offset = (const uint8_t *)bincode->entry_point - 
                          (const uint8_t *)bincode->code;
    clone->entry_point = (uint8_t *)clone->code + entry_offset;

    *cloned = clone;
    return CHOMSKY3_OK;
}

/**
 * Set binary code metadata.
 */
chomsky3_error_t chomsky3_bincode_set_metadata(
    chomsky3_bincode_t *bincode,
    const char *metadata
) {
    if (!bincode) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Free existing metadata */
    free(bincode->metadata);
    bincode->metadata = NULL;

    /* Copy new metadata */
    if (metadata) {
        bincode->metadata = strdup(metadata);
        if (!bincode->metadata) {
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
    }

    return CHOMSKY3_OK;
}

/**
 * Compute CRC32 checksum.
 */
static uint32_t compute_crc32(const uint8_t *data, size_t length) {
    static const uint32_t crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
        0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
        0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
        0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
        0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
        0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
        0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
        0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
        0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
        0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
        0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
        0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
        0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
        0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
        0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
        0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
        0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
        0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
        0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
        0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
        0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
        0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
        0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
        0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
        0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
        0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
        0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
        0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
        0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
        0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
        0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
        0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
        0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
        0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
        0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
        0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
        0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
        0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
        0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
        0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
        0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
        0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
        0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
        0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
        0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/**
 * Serialize metadata to buffer.
 */
static chomsky3_error_t serialize_metadata(
    const chomsky3_bincode_metadata_internal_t *metadata,
    uint8_t **out_data,
    size_t *out_size
) {
    if (!metadata || !out_data || !out_size) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    /* Calculate size */
    size_t size = sizeof(uint32_t) +  /* pattern_length */
                  metadata->pattern_length +
                  sizeof(uint32_t) +  /* num_groups */
                  sizeof(uint32_t) +  /* opt_level */
                  sizeof(uint64_t);   /* compile_time_us */

    /* Allocate buffer */
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        return CHOMSKY3_ERR_OUT_OF_MEMORY;
    }

    /* Write data */
    uint8_t *ptr = buffer;

    /* Pattern length and data */
    *(uint32_t *)ptr = metadata->pattern_length;
    ptr += sizeof(uint32_t);
    if (metadata->pattern_length > 0 && metadata->pattern) {
        memcpy(ptr, metadata->pattern, metadata->pattern_length);
        ptr += metadata->pattern_length;
    }

    /* Other fields */
    *(uint32_t *)ptr = metadata->num_groups;
    ptr += sizeof(uint32_t);
    *(uint32_t *)ptr = metadata->opt_level;
    ptr += sizeof(uint32_t);
    *(uint64_t *)ptr = metadata->compile_time_us;

    *out_data = buffer;
    *out_size = size;

    return CHOMSKY3_OK;
}

/**
 * Deserialize metadata from buffer.
 */
static chomsky3_error_t deserialize_metadata(
    const uint8_t *data,
    size_t size,
    chomsky3_bincode_metadata_internal_t *metadata
) {
    if (!data || !metadata || size < sizeof(uint32_t)) {
        return CHOMSKY3_ERR_INVALID_ARGUMENT;
    }

    memset(metadata, 0, sizeof(*metadata));

    const uint8_t *ptr = data;
    const uint8_t *end = data + size;

    /* Read pattern length */
    if (ptr + sizeof(uint32_t) > end) {
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }
    metadata->pattern_length = *(const uint32_t *)ptr;
    ptr += sizeof(uint32_t);

    /* Read pattern */
    if (metadata->pattern_length > 0) {
        if (ptr + metadata->pattern_length > end) {
            return CHOMSKY3_ERR_INVALID_FORMAT;
        }
        metadata->pattern = malloc(metadata->pattern_length + 1);
        if (!metadata->pattern) {
            return CHOMSKY3_ERR_OUT_OF_MEMORY;
        }
        memcpy(metadata->pattern, ptr, metadata->pattern_length);
        metadata->pattern[metadata->pattern_length] = '\0';
        ptr += metadata->pattern_length;
    }

    /* Read other fields */
    if (ptr + sizeof(uint32_t) * 2 + sizeof(uint64_t) > end) {
        free(metadata->pattern);
        return CHOMSKY3_ERR_INVALID_FORMAT;
    }

    metadata->num_groups = *(const uint32_t *)ptr;
    ptr += sizeof(uint32_t);
    metadata->opt_level = *(const uint32_t *)ptr;
    ptr += sizeof(uint32_t);
    metadata->compile_time_us = *(const uint64_t *)ptr;
    ptr += sizeof(uint64_t);

    return CHOMSKY3_OK;
}
