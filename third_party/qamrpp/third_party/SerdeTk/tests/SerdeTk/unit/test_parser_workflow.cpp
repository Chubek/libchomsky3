#include "../../../include/SerdeTk.hpp"
#include "../../../../AzmaTest/AzmaIDL.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <string>

static std::string read_text(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}

template <typename Fn>
static void expect_parse_error(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const serdetk::ParseError&) {
        threw = true;
    }
    assert(threw);
}

static void test_sktl_and_samples() {
    auto jf = serdetk::sktl::compile_file("stdspec/textual/JSON.sktl");
    auto xf = serdetk::sktl::compile_file("stdspec/textual/XML.sktl");
    auto sf = serdetk::sktl::compile_file("stdspec/textual/S-Expr.sktl");
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

static void test_json_regressions() {
    {
        auto d = serdetk::json::from_string("{\"face\":\"\\uD83D\\uDE00\"}");
        assert(d.root.is_object());
        assert(d.root.as_object().at("face").is_string());
        assert(!d.root.as_object().at("face").as_string().empty());
    }

    expect_parse_error([] {
        (void)serdetk::json::from_string("{\"bad\":\"\x01\"}");
    });
    expect_parse_error([] {
        (void)serdetk::json::from_string("{\"bad\":\"\\uD800\"}");
    });
    expect_parse_error([] {
        (void)serdetk::json::from_string(std::string("{\"bad\"\v:1}", 10));
    });
}

static void test_yaml_regressions() {
    {
        auto d = serdetk::yaml::from_string(
            "root:\n"
            "    child:\n"
            "      - 1\n"
            "      - 2\n"
            "flow: [1, true, {x: 'y'}]\n"
            "block: |\n"
            "  hello\n"
            "  world\n");
        const auto& root = d.root.as_object();
        assert(root.at("root").is_object());
        const auto& child = root.at("root").as_object().at("child").as_array().items;
        assert(child.size() == 2);
        assert(std::get<std::uint64_t>(child[0].data) == 1);
        assert(std::get<std::uint64_t>(child[1].data) == 2);
        const auto& flow = root.at("flow").as_array().items;
        assert(flow.size() == 3);
        assert(std::get<bool>(flow[1].data));
        assert(flow[2].as_object().at("x").as_string() == "y");
        assert(root.at("block").as_string() == "hello\nworld\n");
    }

    expect_parse_error([] {
        (void)serdetk::yaml::from_string("a:\n\tb: 1\n");
    });
}

static void test_xml_regressions() {
    {
        auto d = serdetk::xml::from_string("<root a=\"1\">x&amp;<child>y</child><![CDATA[z]]></root>");
        const auto& root = d.root.as_object().at("root").as_object();
        assert(root.at("@a").as_string() == "1");
        assert(root.at("child").as_string() == "y");
        assert(root.at("#text").as_string() == "x&z");
    }

    expect_parse_error([] {
        (void)serdetk::xml::from_string("<a><b></a>");
    });
    expect_parse_error([] {
        (void)serdetk::xml::from_string("<a/><b/>");
    });
}

static void test_sexpr_regressions() {
    {
        auto d = serdetk::sexpr::from_string("; note\n(root \"x\")\n");
        assert(d.root.is_array());
        const auto& root = d.root.as_array().items;
        assert(root.size() == 2);
        assert(root[0].as_string() == "root");
        assert(root[1].as_string() == "x");
    }

    expect_parse_error([] {
        (void)serdetk::sexpr::from_string("(root");
    });
    expect_parse_error([] {
        (void)serdetk::sexpr::from_string("");
    });
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
    test_json_regressions();
    test_yaml_regressions();
    test_xml_regressions();
    test_sexpr_regressions();
    test_azmaidl_contracts();
    std::puts("parser workflow tests: OK");
    return 0;
}
