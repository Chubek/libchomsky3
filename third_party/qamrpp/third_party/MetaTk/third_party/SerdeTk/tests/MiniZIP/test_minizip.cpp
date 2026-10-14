#include "../../third_party/Catch2/extras/catch_amalgamated.hpp"
#include "../../include/MiniZIP.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct temp_dir {
    std::filesystem::path path;

    temp_dir() {
        path = std::filesystem::temp_directory_path() /
               ("minizip-tests-" + std::to_string(std::hash<std::string>{}("seed")) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }

    ~temp_dir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("MiniZIP backend emits inspectable raw-block zstd frames", "[minizip][backend]") {
    std::string text = "hello zstd subset";
    auto compressed = minizip::backend::zstd_engine::compress(
        std::as_bytes(std::span{text.data(), text.size()}),
        {.algorithm = minizip::backend::codec::zstd});

    REQUIRE(compressed.ok());
    auto info = minizip::backend::zstd_engine::inspect_frame(compressed.bytes);
    REQUIRE(info.has_value());
    CHECK(info->valid);
    CHECK_FALSE(info->skippable);
    REQUIRE(info->content_size.has_value());
    CHECK(*info->content_size == text.size());
    REQUIRE(info->blocks.size() == 1);
    CHECK(info->blocks.front().type == minizip::backend::block_type::raw);

    auto decompressed = minizip::backend::zstd_engine::decompress(compressed.bytes, minizip::backend::codec::zstd);
    REQUIRE(decompressed.ok());
    CHECK(minizip::detail::to_string_lossy(decompressed.bytes) == text);
}

TEST_CASE("MiniZIP archive round-trips generated payloads and manifests", "[minizip][archive]") {
    temp_dir dir;

    auto zipper = minizip::api::zipper::make_zipper();
    zipper.set_codec(minizip::backend::codec::zstd);
    zipper.set_destination(dir.path);
    zipper.set_archive_name("bundle.mz");
    zipper.add_bytes("generated/config.json", minizip::detail::to_bytes(R"({"ok":true})"));
    zipper.create_manifest<minizip::api::INIManifest>();
    zipper.add_to_manifest(".section package");
    zipper.add_to_manifest(".kv name MiniZIP");
    zipper.add_to_manifest(".kv version 1.0.0");

    auto build = zipper.build();
    REQUIRE(build.ok());

    auto opened = minizip::api::extractor::open(dir.path / "bundle.mz");
    REQUIRE(opened.ok());
    auto extractor = std::move(opened.value());

    auto verify = extractor.verify();
    REQUIRE(verify.ok());

    const auto listed = extractor.list_entries();
    REQUIRE(listed.size() == 2);

    auto payload = extractor.extract_bytes("generated/config.json");
    REQUIRE(payload.ok());
    CHECK(minizip::detail::to_string_lossy(payload.value()) == R"({"ok":true})");

    auto manifest = extractor.extract_bytes("meta/package.ini");
    REQUIRE(manifest.ok());
    const auto manifest_text = minizip::detail::to_string_lossy(manifest.value());
    CHECK(manifest_text.find("[package]") != std::string::npos);
    CHECK(manifest_text.find("name=MiniZIP") != std::string::npos);
}

TEST_CASE("MiniZIP MZJL parsing uses DSLtk combinators and scaffolds builds", "[minizip][mzjl]") {
    temp_dir dir;
    const auto source_file = dir.path / "README.md";
    {
        std::ofstream out(source_file);
        out << "readme";
    }

    std::string journal =
        "%MZJL-v1.0.0\n"
        "archive \"journal.mz\"\n"
        "source \"" + source_file.string() + "\"\n"
        "manifest INIManifest\n"
        "manifest_cmd \".section package\"\n"
        "manifest_cmd \".kv name MiniZIP\"\n"
        "save_to \"" + dir.path.string() + "\"\n";

    auto result = minizip::api::make_journal_archiver(journal, minizip::api::MZJL::FromString)
                      .lint()
                      .parse()
                      .validate()
                      .scaffold()
                      .build()
                      .save();

    REQUIRE(result.ok());
    CHECK(std::filesystem::exists(dir.path / "journal.mz"));
}

TEST_CASE("MiniZIP DSL maps to the same zipper execution model", "[minizip][dsl]") {
    temp_dir dir;
    const auto source = dir.path / "note.txt";
    {
        std::ofstream out(source);
        out << "dsl";
    }

    auto plan = minizip::dsl::archive("dsl.mz")
                | minizip::dsl::from(source, minizip::api::options::NonRecursive)
                | minizip::dsl::level(8)
                | minizip::dsl::save_to(dir.path);

    auto build = plan.build();
    REQUIRE(build.ok());
    CHECK(std::filesystem::exists(dir.path / "dsl.mz"));
}
