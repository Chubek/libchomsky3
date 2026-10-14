#pragma once

#include "SerdeTk-Plugin.hpp"
#include "MiniZIP-Plugin.hpp"
#include "../stdplugin/SerdeTk/SchemaRegistry/SchemaRegistry.hpp"
#include "../stdplugin/MiniZIP/PackageIntegrity/PackageIntegrity.hpp"

#include <iostream>

namespace serdetk::frontend {

inline const CompiledFormat& require_format(std::string_view id) {
    auto* format = FormatRegistry::instance().find(std::string(id));
    if (!format) throw FormatError("Unknown format: " + std::string(id));
    return *format;
}

inline void initialize(const std::filesystem::path& manifest_path = "stdspec/MANIFEST.yaml") {
    const auto value = manifest::load_file(manifest_path);
    manifest::register_formats(value, manifest_path.parent_path());
}

inline int execute(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& errors) {
    if (arguments.empty()) {
        errors << "missing command\n";
        return 2;
    }
    try {
        initialize();
        const auto& command = arguments[0];
        if (command == "formats") {
            for (const auto& id : FormatRegistry::instance().names()) output << id << '\n';
            return 0;
        }
        if (command == "plugins") {
            serdetk::plugin::Registry serde_plugins;
            minizip::plugin::Registry minizip_plugins;
            serde_plugins.register_plugin(std::make_shared<schema_registry::Plugin>());
            minizip_plugins.register_plugin(std::make_shared<minizip::package_integrity::Plugin>());
            output << "SerdeTk plugins:\n";
            for (const auto& id : serde_plugins.ids()) output << id << '\n';
            output << "MiniZIP plugins:\n";
            for (const auto& id : minizip_plugins.ids()) output << id << '\n';
            return 0;
        }
        if (command == "schemas") {
            if (arguments.size() != 1 && arguments.size() != 2) throw Error("usage: schemas [format]");
            const auto manifest = manifest::load_file();
            for (const auto& item : manifest.formats) {
                if (arguments.size() == 2 && item.id != arguments[1]) continue;
                output << item.id << ":";
                for (const auto& language : item.schema_languages) output << ' ' << language;
                if (item.schema_required) output << " (required)";
                output << '\n';
            }
            return 0;
        }
        if (command == "validate") {
            if (arguments.size() != 4) throw Error("usage: validate <format> <document> <schema>");
            const auto& format = require_format(arguments[1]);
            const auto document = format.load_file(arguments[2]);
            const auto validator = Validator::from_schema_file(arguments[3], format);
            const auto result = validator->validate(document);
            result.print(output);
            return result.success ? 0 : 1;
        }
        if (command == "serialize" || command == "deserialize" || command == "convert") {
            if (arguments.size() != 4 && !(command == "convert" && arguments.size() == 5)) {
                throw Error("usage: serialize|deserialize <format> <input> <output>; convert <from> <to> <input> <output>");
            }
            const auto& source = require_format(command == "convert" ? arguments[1] : arguments[1]);
            const auto input = command == "convert" ? arguments[3] : arguments[2];
            const auto output_path = command == "convert" ? arguments[4] : arguments[3];
            const auto& target = command == "convert" ? require_format(arguments[2]) : source;
            auto document = source.load_file(input);
            target.dump_file(document, output_path);
            return 0;
        }
        if (command == "pack") {
            if (arguments.size() != 5) throw Error("usage: pack <format> <input> <package.stk> <schema-or-->");
            const auto& format = require_format(arguments[1]);
            package::Contents contents;
            contents.document = format.load_file(arguments[2]);
            contents.format_id = format.name;
            if (arguments[4] != "--") {
                std::ifstream schema(arguments[4]);
                std::ostringstream text;
                text << schema.rdbuf();
                contents.schema = SchemaSource{arguments[4], "", text.str()};
            }
            const auto result = package::create(arguments[3], format, contents);
            if (!result.ok()) throw Error(result.message);
            return 0;
        }
        if (command == "unpack") {
            if (arguments.size() != 3) throw Error("usage: unpack <package.stk> <output>");
            const auto contents = package::extract(arguments[1]);
            require_format(contents.format_id).dump_file(contents.document, arguments[2]);
            return 0;
        }
        if (command == "compress") {
            if (arguments.size() != 3) throw Error("usage: compress <input-path> <archive>");
            auto zipper = minizip::api::zipper::make_zipper();
            zipper.set_archive_name(arguments[2]);
            std::error_code ec;
            if (std::filesystem::is_directory(arguments[1], ec)) zipper.add_directory(arguments[1]);
            else zipper.add_file(arguments[1]);
            const auto result = zipper.build();
            if (!result.ok()) throw Error(result.message);
            return 0;
        }
        if (command == "decompress") {
            if (arguments.size() != 3) throw Error("usage: decompress <archive> <output-directory>");
            auto opened = minizip::api::extractor::open(arguments[1]);
            if (!opened.ok()) throw Error(opened.message());
            const auto result = opened.value().extract_all_to(arguments[2]);
            if (!result.ok()) throw Error(result.message);
            return 0;
        }
        throw Error("unsupported command: " + command);
    } catch (const std::exception& error) {
        errors << error.what() << '\n';
        return 1;
    }
}

} // namespace serdetk::frontend
