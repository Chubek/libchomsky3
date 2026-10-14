#include "../../SerdeTk.hpp"
#include "../../AzmaTest/AzmaIDL.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string read_text(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}

static void test_sktl_and_samples() {
    auto jf = serdetk::sktl::compile_file("std/textual/JSON.sktl");
    auto xf = serdetk::sktl::compile_file("std/textual/XML.sktl");
    auto sf = serdetk::sktl::compile_file("std/textual/S-Expr.sktl");
    assert(jf.name == "JSON" && xf.name == "XML" && sf.name == "S-Expr");

    const char* json[] = {"samples/JSON/scientific-dataset.json","samples/JSON/infrstructure-config.json","samples/JSON/weird-edge-cases.json"};
    const char* xml[] = {"samples/XML/mixed-content-cdata.xml","samples/XML/enterprise-data.xml","samples/XML/namespaces.xml"};
    const char* sexpr[] = {"samples/S-Expr/config-format.sexp","samples/S-Expr/compiler-ir.sexp","samples/S-Expr/lisp-ast.sexp"};

    for (auto f : json) { auto d = serdetk::json::from_string(read_text(f)); assert(!d.root.is_null()); }
    for (auto f : xml) {
        auto d = serdetk::xml::from_string(read_text(f));
        assert(d.root.is_object());
    }
    {
        auto d = serdetk::xml::from_string(read_text("samples/XML/enterprise-data.xml"));
        assert(d.root.is_object());
        const auto& top = d.root.as_object();
        assert(top.contains("enterprise"));
        assert(top.fields.at("enterprise").is_object());
    }
    {
        auto d = serdetk::xml::from_string(read_text("samples/XML/mixed-content-cdata.xml"));
        const auto& top = d.root.as_object();
        assert(top.contains("document"));
        const auto& doc = top.fields.at("document");
        assert(doc.is_object());
        assert(doc.as_object().contains("script"));
    }
    {
        auto d = serdetk::xml::from_string(read_text("samples/XML/namespaces.xml"));
        const auto& top = d.root.as_object();
        assert(top.contains("root"));
        assert(top.fields.at("root").is_object());
    }
    for (auto f : sexpr) { auto d = serdetk::sexpr::from_string(read_text(f)); assert(!d.root.is_null()); }
}

static void test_azmaidl_contracts() {
    const char* in = "metadata author=\"azma\"\nconfig retries=3\n";
    AzmaIDLSource src{"<unit>",(const uint8_t*)in,strlen(in)};
    AzmaIDLParseOptions opt{AZMA_IDL_PARSE_COLLECT_DIAGNOSTICS, azma_allocator_default(), NULL};
    AzmaIDLDocument* doc = NULL;
    AzmaStatus st = azma_idl_parse(&src, &opt, &doc);
    assert(st == AZMA_STATUS_OK && doc != NULL);
    assert(azma_idl_document_decl_count(doc) == 2u);
    azma_idl_document_destroy(doc);
}

int main() {
    test_sktl_and_samples();
    test_azmaidl_contracts();
    std::puts("parser workflow tests: OK");
    return 0;
}
