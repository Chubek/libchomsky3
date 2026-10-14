#include "AzmaIDL.h"

#include <stdio.h>
#include <stdlib.h>

static int azma_read_file(const char *path, uint8_t **out_data, size_t *out_size) {
    FILE *f;
    long size;
    uint8_t *buf;

    if (!path || !out_data || !out_size) {
        return 0;
    }

    *out_data = NULL;
    *out_size = 0;

    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    buf = (uint8_t *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(f);
        return 0;
    }

    if ((size_t)size != fread(buf, 1, (size_t)size, f)) {
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    buf[size] = 0;

    *out_data = buf;
    *out_size = (size_t)size;
    return 1;
}

int main(int argc, char **argv) {
    AzmaIDLSource src;
    AzmaIDLParseOptions options;
    AzmaIDLDocument *doc = NULL;
    const AzmaIDLDiagnosticList *diags;
    uint8_t *data = NULL;
    size_t size = 0;
    AzmaStatus st;
    size_t i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <input.azmaidl>\n", argv[0]);
        return 2;
    }

    if (!azma_read_file(argv[1], &data, &size)) {
        fprintf(stderr, "error: could not read %s\n", argv[1]);
        return 2;
    }

    src.path = argv[1];
    src.data = data;
    src.size = size;

    options.flags = AZMA_IDL_PARSE_COLLECT_DIAGNOSTICS | AZMA_IDL_PARSE_RECOVER;
    options.allocator = azma_allocator_default();
    options.user = NULL;

    st = azma_idl_parse(&src, &options, &doc);
    if (!doc) {
        fprintf(stderr, "parse failed: %s\n", azma_status_string(st));
        free(data);
        return 1;
    }

    diags = azma_idl_document_diagnostics(doc);
    if (diags) {
        for (i = 0; i < diags->count; ++i) {
            const AzmaIDLDiagnostic *d = &diags->items[i];
            fprintf(stderr, "%s:%zu:%zu: %s: %.*s\n",
                    src.path ? src.path : "<input>",
                    d->where.begin.line,
                    d->where.begin.column,
                    azma_idl_diag_severity_name(d->severity),
                    (int)d->message.size,
                    d->message.data ? d->message.data : "");
        }
    }

    azma_idl_dump_document(stdout, doc);
    azma_idl_document_destroy(doc);
    free(data);

    return st == AZMA_STATUS_OK ? 0 : 1;
}
