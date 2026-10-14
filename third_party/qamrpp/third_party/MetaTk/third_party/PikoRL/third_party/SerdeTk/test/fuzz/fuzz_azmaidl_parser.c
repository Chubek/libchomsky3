#include "../../AzmaTest/AzmaFuzz.h"
#include "../../AzmaTest/AzmaIDL.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FuzzCtx {
    int save_failures;
} FuzzCtx;

static AzmaStatus target_parse(void *user, const uint8_t *data, size_t size) {
    AzmaIDLSource src;
    AzmaIDLParseOptions opt;
    AzmaIDLDocument *doc = NULL;
    AzmaStatus st;
    (void)user;
    src.path = "<fuzz>";
    src.data = data;
    src.size = size;
    opt.flags = AZMA_IDL_PARSE_COLLECT_DIAGNOSTICS | AZMA_IDL_PARSE_RECOVER;
    opt.allocator = azma_allocator_default();
    opt.user = NULL;
    st = azma_idl_parse(&src, &opt, &doc);
    azma_idl_document_destroy(doc);
    if (st == AZMA_STATUS_OK || st == AZMA_STATUS_PARSE_ERROR) {
        return AZMA_STATUS_OK;
    }
    return st;
}

static int load_file(const char *path, uint8_t **out, size_t *out_sz) {
    FILE *f = fopen(path, "rb");
    long n;
    uint8_t *buf;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    buf = (uint8_t *)malloc((size_t)n);
    if (!buf) { fclose(f); return 0; }
    if ((size_t)n != fread(buf, 1, (size_t)n, f)) { free(buf); fclose(f); return 0; }
    fclose(f);
    *out = buf; *out_sz = (size_t)n; return 1;
}

int main(void) {
    const char *seed_paths[] = {
        "samples/JSON/scientific-dataset.json",
        "samples/JSON/infrstructure-config.json",
        "samples/JSON/weird-edge-cases.json",
        "samples/XML/mixed-content-cdata.xml",
        "samples/XML/enterprise-data.xml",
        "samples/XML/namespaces.xml",
        "samples/S-Expr/config-format.sexp",
        "samples/S-Expr/compiler-ir.sexp",
        "samples/S-Expr/lisp-ast.sexp"
    };
    AzmaFuzzInput corpus[32];
    uint8_t *owned[32];
    size_t corpus_count = 0;
    AzmaFuzzOptions opt = azma_fuzz_options_default();
    AzmaFuzzStats stats;
    AzmaStatus st;
    size_t i;

    memset(owned, 0, sizeof(owned));
    for (i = 0; i < sizeof(seed_paths)/sizeof(seed_paths[0]); ++i) {
        uint8_t *data = NULL; size_t sz = 0;
        if (!load_file(seed_paths[i], &data, &sz)) continue;
        owned[corpus_count] = data;
        corpus[corpus_count++] = azma_fuzz_input_from_bytes(data, sz);
    }
    {
        const char *extra[] = {
            "test/corpus/parser/xml_doctype.xml",
            "test/corpus/parser/xml_cdata.xml",
            "test/corpus/parser/xml_pi_inside.xml",
            "test/corpus/parser/xml_attr_slash.xml"
        };
        for (i = 0; i < sizeof(extra)/sizeof(extra[0]); ++i) {
            uint8_t *data = NULL; size_t sz = 0;
            if (!load_file(extra[i], &data, &sz)) continue;
            if (corpus_count >= sizeof(corpus)/sizeof(corpus[0])) { free(data); break; }
            owned[corpus_count] = data;
            corpus[corpus_count++] = azma_fuzz_input_from_bytes(data, sz);
        }
    }

    opt.seed = 0xA11CEBADC0DE1234ull;
    opt.iterations = 300;
    opt.max_input_size = 8192;
    opt.stop_on_failure = 1;

    st = azma_fuzz_run(target_parse, NULL, corpus, corpus_count, &opt, &stats);
    azma_fuzz_print_stats(stdout, &stats);

    for (i = 0; i < corpus_count; ++i) free(owned[i]);
    return st == AZMA_STATUS_OK ? 0 : 1;
}
