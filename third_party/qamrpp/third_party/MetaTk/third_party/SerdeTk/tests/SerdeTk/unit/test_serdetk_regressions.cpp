#include "../../SerdeTk.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

using namespace serdetk;

static void test_convert_no_placeholder_loss() {
    auto obj = std::make_shared<Object>();
    obj->set("k", Value("v"));
    Document d;
    d.root = Value(obj);

    conversion::Report report;
    auto out = Convert<yaml::JSONAdapter>::run(d, &report);
    (void)out;
    for (auto loss : report.losses) {
        assert(loss != conversion::LossKind::Placeholder);
    }
}

static void test_query_invalid_index_deterministic() {
    auto arr = std::make_shared<Array>();
    arr->push(Value(1));
    arr->push(Value(2));
    Document d;
    d.root = Value(arr);

    auto r1 = STKQ::run(d, "root[nope]");
    auto r2 = STKQ::run(d, "root[nope]");
    assert(r1.values.empty());
    assert(r2.values.empty());
}

static void test_cbor_sktl_compile_and_registry() {
    auto fmt = sktl::compile_file("std/binary/CBOR.sktl");
    assert(fmt.name == "CBOR");
    assert(fmt.is_binary());
    assert(!fmt.extensions.empty() && fmt.extensions[0] == ".cbor");

    register_builtin_formats();
    auto* reg = FormatRegistry::instance().find("CBOR");
    assert(reg != nullptr);
    assert(reg->is_binary());
}

static void test_cbor_roundtrip_and_edges() {
    auto root = std::make_shared<Object>();
    auto arr = std::make_shared<Array>();
    arr->push(Value(nullptr));
    arr->push(Value(true));
    arr->push(Value(std::int64_t(-1)));
    arr->push(Value(std::uint64_t(18446744073709551615ULL)));
    root->set("arr", Value(arr));
    root->set("s", Value(std::string("hello")));
    root->set("b", Value(Binary{{0x00, 0xff, 0x10}}));
    root->set("d", Value(3.5));

    Document d;
    d.root = Value(root);

    auto bytes1 = cbor::format().dump_bytes(d);
    auto bytes2 = cbor::format().dump_bytes(d);
    assert(bytes1 == bytes2);

    auto parsed = cbor::format().load_bytes(bytes1);
    assert(parsed.root.is_object());
    assert(parsed.root.as_object().contains("arr"));
    assert(parsed.root.as_object().contains("s"));
    assert(parsed.root.as_object().contains("b"));
    assert(parsed.root.as_object().contains("d"));
    assert(parsed.root.as_object().at("arr").is_array());
    assert(parsed.root.as_object().at("arr").as_array().items.size() == 4);
    assert(parsed.root.as_object().at("b").is_binary());
    assert(parsed.root.as_object().at("d").is_double());
    assert(std::fabs(std::get<double>(parsed.root.as_object().at("d").data) - 3.5) < 1e-12);
}

static void test_cbor_malformed_inputs() {
    bool threw = false;
    try {
        cbor::format().load_bytes({});
    } catch (const ParseError&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        cbor::format().load_bytes({0x63, 'a'});
    } catch (const ParseError&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        cbor::format().load_bytes({0xa1, 0x01, 0x01});
    } catch (const ParseError&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        cbor::format().load_bytes({0xfb, 0x3f, 0xf0});
    } catch (const ParseError&) {
        threw = true;
    }
    assert(threw);
}

static void test_cbor_float_decode() {
    auto d = cbor::format().load_bytes({0xfb, 0x3f, 0xf8, 0, 0, 0, 0, 0, 0});
    assert(d.root.is_double());
    assert(std::fabs(std::get<double>(d.root.data) - 1.5) < 1e-12);
}

int main() {
    test_convert_no_placeholder_loss();
    test_query_invalid_index_deterministic();
    test_cbor_sktl_compile_and_registry();
    test_cbor_roundtrip_and_edges();
    test_cbor_malformed_inputs();
    test_cbor_float_decode();
    std::puts("serdetk regressions: OK");
    return 0;
}
