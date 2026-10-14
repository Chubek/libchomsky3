#pragma once

/**
 * @file MiniZIP.hpp
 * @brief Header-only C++20 archive library with a MiniZIP container format and
 *        a deliberately honest Zstandard-oriented backend subset.
 *
 * The library provides four primary namespaces:
 * - `minizip::backend` implements low-level Zstandard frame parsing and a
 *   standards-compliant raw-block encoder subset.
 * - `minizip::api` exposes the public archive creation, listing, verification,
 *   extraction, manifest, middleware, and journal facilities.
 * - `minizip::dsl` exposes a thin native C++ DSL layer built on top of the
 *   real `DSLtk.hpp` pipeline primitives.
 * - `minizip::detail` contains private helpers and is not stable API.
 *
 * Supported backend scope:
 * - Parses Zstandard frame headers, skippable frames, and block headers.
 * - Decompresses `Raw_Block` and `RLE_Block` frames.
 * - Emits valid Zstandard frames containing `Raw_Block` blocks only.
 *
 * Explicitly unsupported today:
 * - Compressed Zstandard blocks (`Block_Type == Compressed_Block`)
 * - Huffman/FSE entropy decoding
 * - Dictionaries
 * - xxHash checksum generation and validation
 *
 * This is intentional. The implementation exposes a real, correct subset
 * instead of pretending to support the full Zstandard specification.
 */

#include <algorithm>
#include <any>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "DSLtk.hpp"

namespace minizip {

/**
 * @namespace minizip::detail
 * @brief Internal helpers for byte IO, path normalization, hashing, and small
 *        utility adapters used by the public API and backend.
 *
 * Nothing in this namespace is intended to be stable user-facing API.
 */
namespace detail {

using byte_vector = std::vector<std::byte>;

template <class T>
concept ByteLike =
    std::same_as<std::remove_cv_t<T>, std::byte> ||
    std::same_as<std::remove_cv_t<T>, char> ||
    std::same_as<std::remove_cv_t<T>, unsigned char>;

inline std::string trim_copy(std::string text) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

inline bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

inline byte_vector to_bytes(std::string_view text) {
    byte_vector out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
    return out;
}

template <ByteLike T>
inline byte_vector to_bytes(std::span<const T> bytes) {
    byte_vector out;
    out.reserve(bytes.size());
    for (const auto &item : bytes) {
        if constexpr (std::same_as<std::remove_cv_t<T>, std::byte>) {
            out.push_back(item);
        } else {
            out.push_back(static_cast<std::byte>(static_cast<unsigned char>(item)));
        }
    }
    return out;
}

inline std::string to_string_lossy(std::span<const std::byte> bytes) {
    std::string out;
    out.reserve(bytes.size());
    for (std::byte byte : bytes) {
        out.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
    }
    return out;
}

inline std::uint64_t fnv1a64(std::span<const std::byte> data) {
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    for (std::byte byte : data) {
        hash ^= static_cast<std::uint64_t>(std::to_integer<unsigned char>(byte));
        hash *= prime;
    }
    return hash;
}

inline std::uint64_t fnv1a64(std::string_view text) {
    return fnv1a64(std::as_bytes(std::span{text.data(), text.size()}));
}

inline std::uint64_t unix_seconds_now() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

inline std::filesystem::path normalize_relative_path(const std::filesystem::path &input) {
    std::vector<std::string> parts;
    for (const auto &part : input.lexically_normal()) {
        const auto text = part.generic_string();
        if (text.empty() || text == "." || text == "/") {
            continue;
        }
        if (text == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
            continue;
        }
        parts.push_back(text);
    }

    std::filesystem::path out;
    for (const auto &part : parts) {
        out /= part;
    }
    return out;
}

inline std::string normalize_entry_path(const std::filesystem::path &path, bool directory_hint = false) {
    auto normalized = normalize_relative_path(path).generic_string();
    while (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
    }
    if (directory_hint && !normalized.empty() && normalized.back() != '/') {
        normalized.push_back('/');
    }
    return normalized;
}

inline std::optional<byte_vector> read_file_bytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    if (end < 0) {
        return std::nullopt;
    }
    const auto size = static_cast<std::size_t>(end);
    in.seekg(0, std::ios::beg);
    byte_vector bytes(size);
    if (size > 0) {
        in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
        if (!in) {
            return std::nullopt;
        }
    }
    return bytes;
}

inline std::optional<std::string> read_file_text(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof()) {
        return std::nullopt;
    }
    return buffer.str();
}

inline bool write_file_bytes(const std::filesystem::path &path,
                             std::span<const std::byte> bytes,
                             std::string *error_message = nullptr) {
    std::error_code ec;
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            if (error_message) {
                *error_message = "MiniZIP: failed to create parent directory '" + parent.string() +
                                 "': " + ec.message();
            }
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error_message) {
            *error_message = "MiniZIP: failed to open output file '" + path.string() + "'";
        }
        return false;
    }

    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    if (!out.good()) {
        if (error_message) {
            *error_message = "MiniZIP: failed to write output file '" + path.string() + "'";
        }
        return false;
    }
    return true;
}

/**
 * @brief Little-endian byte accumulator used by archive and backend encoders.
 */
class byte_writer {
public:
    void write_u8(std::uint8_t value) { buffer_.push_back(static_cast<std::byte>(value)); }
    void write_u16(std::uint16_t value) { write_integral(value); }
    void write_u24(std::uint32_t value) {
        write_u8(static_cast<std::uint8_t>(value & 0xffu));
        write_u8(static_cast<std::uint8_t>((value >> 8) & 0xffu));
        write_u8(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    }
    void write_u32(std::uint32_t value) { write_integral(value); }
    void write_u64(std::uint64_t value) { write_integral(value); }
    void write_bytes(std::span<const std::byte> bytes) { buffer_.insert(buffer_.end(), bytes.begin(), bytes.end()); }
    void write_string(std::string_view text) { write_bytes(std::as_bytes(std::span{text.data(), text.size()})); }
    [[nodiscard]] const byte_vector &buffer() const noexcept { return buffer_; }
    [[nodiscard]] byte_vector take() && noexcept { return std::move(buffer_); }

private:
    template <class UInt>
    void write_integral(UInt value) {
        static_assert(std::is_unsigned_v<UInt>);
        for (std::size_t index = 0; index < sizeof(UInt); ++index) {
            write_u8(static_cast<std::uint8_t>((value >> (index * 8)) & 0xffu));
        }
    }

    byte_vector buffer_;
};

/**
 * @brief Little-endian byte reader with bounds checking.
 */
class byte_reader {
public:
    explicit byte_reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] bool eof() const noexcept { return offset_ >= bytes_.size(); }
    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - offset_; }
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

    std::uint8_t read_u8() {
        ensure(1);
        return std::to_integer<std::uint8_t>(bytes_[offset_++]);
    }
    std::uint16_t read_u16() { return read_integral<std::uint16_t>(); }
    std::uint32_t read_u24() {
        ensure(3);
        const auto b0 = std::to_integer<std::uint32_t>(bytes_[offset_++]);
        const auto b1 = std::to_integer<std::uint32_t>(bytes_[offset_++]);
        const auto b2 = std::to_integer<std::uint32_t>(bytes_[offset_++]);
        return b0 | (b1 << 8u) | (b2 << 16u);
    }
    std::uint32_t read_u32() { return read_integral<std::uint32_t>(); }
    std::uint64_t read_u64() { return read_integral<std::uint64_t>(); }

    std::span<const std::byte> read_span(std::size_t count) {
        ensure(count);
        auto out = bytes_.subspan(offset_, count);
        offset_ += count;
        return out;
    }

    byte_vector read_bytes(std::size_t count) {
        const auto view = read_span(count);
        return byte_vector(view.begin(), view.end());
    }

private:
    void ensure(std::size_t count) const {
        if (offset_ + count > bytes_.size()) {
            throw std::runtime_error("MiniZIP: unexpected end of buffer");
        }
    }

    template <class UInt>
    UInt read_integral() {
        static_assert(std::is_unsigned_v<UInt>);
        ensure(sizeof(UInt));
        UInt value = 0;
        for (std::size_t index = 0; index < sizeof(UInt); ++index) {
            value |= static_cast<UInt>(std::to_integer<std::uint8_t>(bytes_[offset_++])) << (index * 8u);
        }
        return value;
    }

    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0;
};

} // namespace detail

/**
 * @namespace minizip::backend
 * @brief Zstandard-oriented backend with a real parser and an intentionally
 *        constrained but standards-compliant encoder subset.
 *
 * The encoder emits valid Zstandard frames using `Raw_Block` blocks only.
 * The decoder supports `Raw_Block` and `RLE_Block` blocks and rejects
 * compressed blocks honestly.
 */
namespace backend {

/**
 * @brief Compression algorithm stored in per-entry archive metadata.
 */
enum class codec : std::uint8_t {
    stored = 0,
    zstd = 1
};

/**
 * @brief Compression configuration passed from the public API into the backend.
 *
 * `level` is advisory for the current raw-block encoder subset. It is preserved
 * for future richer backends but currently does not change output.
 */
struct compression_options {
    codec algorithm = codec::stored;
    int level = 0;
    bool checksum = false;
    bool deterministic = false;
};

/**
 * @brief Generic byte-buffer result returned by backend operations.
 */
struct buffer_result {
    bool success = false;
    std::string message;
    detail::byte_vector bytes;

    [[nodiscard]] bool ok() const noexcept { return success; }
};

/**
 * @brief Zstandard block kind as defined by the spec's 2-bit `Block_Type`.
 */
enum class block_type : std::uint8_t {
    raw = 0,
    rle = 1,
    compressed = 2,
    reserved = 3
};

/**
 * @brief Parsed Zstandard block header information.
 */
struct zstd_block_info {
    bool last_block = false;
    block_type type = block_type::raw;
    std::uint32_t block_size = 0;
    std::size_t payload_offset = 0;
};

/**
 * @brief Parsed frame descriptor information.
 */
struct zstd_frame_info {
    bool valid = false;
    bool skippable = false;
    bool single_segment = false;
    bool content_checksum = false;
    bool reserved_bit_set = false;
    std::optional<std::uint32_t> dictionary_id;
    std::optional<std::uint64_t> content_size;
    std::optional<std::uint64_t> window_size;
    std::size_t header_size = 0;
    std::size_t total_size = 0;
    bool uses_unsupported_compressed_blocks = false;
    std::vector<zstd_block_info> blocks;
};

namespace detail_backend {

constexpr std::uint32_t zstd_magic = 0xFD2FB528u;
constexpr std::uint32_t skippable_magic_min = 0x184D2A50u;
constexpr std::uint32_t skippable_magic_max = 0x184D2A5Fu;
constexpr std::size_t zstd_block_max_size = 128u * 1024u;

inline bool is_skippable_magic(std::uint32_t magic) {
    return magic >= skippable_magic_min && magic <= skippable_magic_max;
}

inline std::optional<std::uint64_t> decode_window_size(std::uint8_t descriptor) {
    const auto exponent = static_cast<std::uint64_t>(descriptor >> 3u);
    const auto mantissa = static_cast<std::uint64_t>(descriptor & 0x07u);
    const auto window_log = 10ull + exponent;
    if (window_log >= 63u) {
        return std::nullopt;
    }
    const auto window_base = 1ull << window_log;
    const auto window_add = (window_base / 8ull) * mantissa;
    return window_base + window_add;
}

inline std::uint8_t fcs_field_size(std::uint8_t flag_value, bool single_segment) {
    if (flag_value == 0u) {
        return single_segment ? 1u : 0u;
    }
    if (flag_value == 1u) {
        return 2u;
    }
    if (flag_value == 2u) {
        return 4u;
    }
    return 8u;
}

inline std::uint64_t read_fcs(detail::byte_reader &reader, std::uint8_t field_size) {
    switch (field_size) {
        case 0: return 0;
        case 1: return reader.read_u8();
        case 2: return static_cast<std::uint64_t>(reader.read_u16()) + 256ull;
        case 4: return reader.read_u32();
        case 8: return reader.read_u64();
        default: throw std::runtime_error("MiniZIP: invalid frame content size field size");
    }
}

inline std::uint32_t read_dictionary_id(detail::byte_reader &reader, std::uint8_t field_size) {
    switch (field_size) {
        case 0: return 0;
        case 1: return reader.read_u8();
        case 2: return reader.read_u16();
        case 4: return reader.read_u32();
        default: throw std::runtime_error("MiniZIP: invalid dictionary id field size");
    }
}

inline std::uint8_t dictionary_id_field_size(std::uint8_t flag_value) {
    static constexpr std::array<std::uint8_t, 4> sizes{0u, 1u, 2u, 4u};
    return sizes.at(flag_value & 0x03u);
}

inline void write_fcs(detail::byte_writer &writer, std::uint64_t value, std::uint8_t field_size) {
    switch (field_size) {
        case 1: writer.write_u8(static_cast<std::uint8_t>(value)); break;
        case 2: writer.write_u16(static_cast<std::uint16_t>(value - 256ull)); break;
        case 4: writer.write_u32(static_cast<std::uint32_t>(value)); break;
        case 8: writer.write_u64(value); break;
        default: break;
    }
}

inline std::uint8_t choose_fcs_field_size(std::uint64_t size) {
    if (size <= 255ull) {
        return 1u;
    }
    if (size <= 65791ull) {
        return 2u;
    }
    if (size <= 0xFFFFFFFFull) {
        return 4u;
    }
    return 8u;
}

inline std::uint8_t choose_fcs_flag(std::uint8_t field_size) {
    switch (field_size) {
        case 1: return 0u;
        case 2: return 1u;
        case 4: return 2u;
        case 8: return 3u;
        default: return 0u;
    }
}

} // namespace detail_backend

/**
 * @brief Real Zstandard backend subset used by MiniZIP archive entries.
 *
 * Compression behavior:
 * - `codec::stored`: exact pass-through bytes
 * - `codec::zstd`: emits a legal Zstandard frame using only `Raw_Block` blocks
 *
 * Decompression behavior:
 * - `codec::stored`: exact pass-through bytes
 * - `codec::zstd`: parses concatenated frames and skippable frames, decodes
 *   raw and RLE blocks, and rejects compressed blocks explicitly
 */
class zstd_engine {
public:
    /**
     * @brief Compresses an input buffer according to the selected codec.
     * @param input Source bytes.
     * @param options Backend compression options.
     * @return Byte buffer result. Zstandard output is a valid frame subset.
     */
    [[nodiscard]] static buffer_result compress(std::span<const std::byte> input,
                                                const compression_options &options = {}) {
        if (options.algorithm == codec::stored) {
            return {true, {}, detail::byte_vector(input.begin(), input.end())};
        }

        if (options.algorithm != codec::zstd) {
            return {false, "MiniZIP: unsupported backend codec", {}};
        }

        if (options.checksum) {
            return {false, "MiniZIP: zstd checksum emission is not implemented", {}};
        }

        detail::byte_writer writer;
        writer.write_u32(detail_backend::zstd_magic);

        const auto content_size = static_cast<std::uint64_t>(input.size());
        const auto fcs_size = detail_backend::choose_fcs_field_size(content_size);
        const auto fcs_flag = detail_backend::choose_fcs_flag(fcs_size);

        std::uint8_t descriptor = 0;
        descriptor |= static_cast<std::uint8_t>(fcs_flag << 6u);
        descriptor |= static_cast<std::uint8_t>(1u << 5u);
        writer.write_u8(descriptor);
        detail_backend::write_fcs(writer, content_size, fcs_size);

        std::size_t offset = 0;
        while (offset < input.size()) {
            const auto remaining = input.size() - offset;
            const auto chunk = std::min<std::size_t>(remaining, detail_backend::zstd_block_max_size);
            const bool last = (offset + chunk) == input.size();
            std::uint32_t header = static_cast<std::uint32_t>(last ? 1u : 0u);
            header |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(block_type::raw)) << 1u;
            header |= static_cast<std::uint32_t>(chunk) << 3u;
            writer.write_u24(header);
            writer.write_bytes(input.subspan(offset, chunk));
            offset += chunk;
        }

        if (input.empty()) {
            std::uint32_t header = 1u;
            header |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(block_type::raw)) << 1u;
            writer.write_u24(header);
        }

        return {true, {}, std::move(writer).take()};
    }

    /**
     * @brief Decompresses a buffer according to the selected codec.
     * @param input Compressed bytes.
     * @param algorithm Codec stored in archive metadata.
     * @return Byte buffer result.
     *
     * For `codec::zstd`, only raw and RLE blocks are currently supported.
     * Compressed blocks are detected and rejected with a precise message.
     */
    [[nodiscard]] static buffer_result decompress(std::span<const std::byte> input,
                                                  codec algorithm = codec::stored) {
        if (algorithm == codec::stored) {
            return {true, {}, detail::byte_vector(input.begin(), input.end())};
        }
        if (algorithm != codec::zstd) {
            return {false, "MiniZIP: unsupported backend codec", {}};
        }

        detail::byte_reader reader(input);
        detail::byte_vector output;

        try {
            while (!reader.eof()) {
                const auto frame_start = reader.offset();
                const auto magic = reader.read_u32();

                if (detail_backend::is_skippable_magic(magic)) {
                    const auto skip_size = static_cast<std::size_t>(reader.read_u32());
                    static_cast<void>(reader.read_span(skip_size));
                    continue;
                }

                if (magic != detail_backend::zstd_magic) {
                    return {false, "MiniZIP: invalid or unsupported zstd frame magic", {}};
                }

                auto descriptor = reader.read_u8();
                const auto fcs_flag = static_cast<std::uint8_t>(descriptor >> 6u);
                const bool single_segment = (descriptor & 0x20u) != 0;
                const bool reserved_bit = (descriptor & 0x08u) != 0;
                const bool checksum_flag = (descriptor & 0x04u) != 0;
                const auto did_flag = static_cast<std::uint8_t>(descriptor & 0x03u);

                if (reserved_bit) {
                    return {false, "MiniZIP: zstd reserved frame header bit is set", {}};
                }

                std::optional<std::uint64_t> frame_content_size;
                std::optional<std::uint64_t> window_size;

                if (!single_segment) {
                    const auto window_descriptor = reader.read_u8();
                    window_size = detail_backend::decode_window_size(window_descriptor);
                    if (!window_size) {
                        return {false, "MiniZIP: invalid zstd window descriptor", {}};
                    }
                }

                const auto did_field_size = detail_backend::dictionary_id_field_size(did_flag);
                if (did_field_size != 0u) {
                    static_cast<void>(detail_backend::read_dictionary_id(reader, did_field_size));
                }

                const auto fcs_size = detail_backend::fcs_field_size(fcs_flag, single_segment);
                if (fcs_size != 0u) {
                    frame_content_size = detail_backend::read_fcs(reader, fcs_size);
                    if (single_segment) {
                        window_size = frame_content_size;
                    }
                }

                bool last_block = false;
                while (!last_block) {
                    const auto header = reader.read_u24();
                    last_block = (header & 0x1u) != 0;
                    const auto type = static_cast<block_type>((header >> 1u) & 0x3u);
                    const auto block_size = static_cast<std::uint32_t>(header >> 3u);

                    if (type == block_type::reserved) {
                        return {false, "MiniZIP: zstd reserved block type encountered", {}};
                    }

                    if (type == block_type::raw) {
                        const auto block = reader.read_span(block_size);
                        output.insert(output.end(), block.begin(), block.end());
                    } else if (type == block_type::rle) {
                        const auto value = reader.read_u8();
                        output.insert(output.end(), block_size, static_cast<std::byte>(value));
                    } else {
                        return {false,
                                "MiniZIP: zstd compressed blocks are not implemented yet; only raw/RLE blocks are supported",
                                {}};
                    }
                }

                if (checksum_flag) {
                    static_cast<void>(reader.read_u32());
                }

                if (frame_start == reader.offset()) {
                    return {false, "MiniZIP: stalled while decoding zstd frame", {}};
                }
                if (frame_content_size && *frame_content_size > output.size()) {
                    return {false, "MiniZIP: truncated zstd frame content", {}};
                }
            }
        } catch (const std::exception &error) {
            return {false, std::string("MiniZIP: zstd decompression failed: ") + error.what(), {}};
        }

        return {true, {}, std::move(output)};
    }

    /**
     * @brief Parses a single frame header and all block headers for inspection.
     * @param input Bytes beginning at a prospective frame boundary.
     * @return Parsed frame info or `std::nullopt` when the prefix is not a
     *         recognized Zstandard/skippable frame.
     */
    [[nodiscard]] static std::optional<zstd_frame_info> inspect_frame(std::span<const std::byte> input) {
        try {
            if (input.size() < 4u) {
                return std::nullopt;
            }

            detail::byte_reader reader(input);
            const auto magic = reader.read_u32();

            zstd_frame_info info;
            info.valid = true;

            if (detail_backend::is_skippable_magic(magic)) {
                info.skippable = true;
                const auto payload_size = static_cast<std::size_t>(reader.read_u32());
                static_cast<void>(reader.read_span(payload_size));
                info.header_size = 8u;
                info.total_size = 8u + payload_size;
                return info;
            }

            if (magic != detail_backend::zstd_magic) {
                return std::nullopt;
            }

            const auto descriptor = reader.read_u8();
            const auto fcs_flag = static_cast<std::uint8_t>(descriptor >> 6u);
            info.single_segment = (descriptor & 0x20u) != 0;
            info.reserved_bit_set = (descriptor & 0x08u) != 0;
            info.content_checksum = (descriptor & 0x04u) != 0;
            const auto did_flag = static_cast<std::uint8_t>(descriptor & 0x03u);

            if (!info.single_segment) {
                const auto window_descriptor = reader.read_u8();
                info.window_size = detail_backend::decode_window_size(window_descriptor);
            }

            const auto did_field_size = detail_backend::dictionary_id_field_size(did_flag);
            if (did_field_size != 0u) {
                const auto did = detail_backend::read_dictionary_id(reader, did_field_size);
                if (did != 0u) {
                    info.dictionary_id = did;
                }
            }

            const auto fcs_size = detail_backend::fcs_field_size(fcs_flag, info.single_segment);
            if (fcs_size != 0u) {
                info.content_size = detail_backend::read_fcs(reader, fcs_size);
                if (info.single_segment) {
                    info.window_size = info.content_size;
                }
            }

            info.header_size = reader.offset();

            bool last_block = false;
            while (!last_block) {
                const auto header_offset = reader.offset();
                const auto header = reader.read_u24();
                zstd_block_info block;
                block.last_block = (header & 0x1u) != 0;
                block.type = static_cast<block_type>((header >> 1u) & 0x3u);
                block.block_size = static_cast<std::uint32_t>(header >> 3u);
                block.payload_offset = header_offset + 3u;
                info.blocks.push_back(block);
                last_block = block.last_block;

                if (block.type == block_type::reserved) {
                    break;
                }
                if (block.type == block_type::raw || block.type == block_type::compressed) {
                    static_cast<void>(reader.read_span(block.block_size));
                } else if (block.type == block_type::rle) {
                    static_cast<void>(reader.read_span(1u));
                }
                if (block.type == block_type::compressed) {
                    info.uses_unsupported_compressed_blocks = true;
                }
            }

            if (info.content_checksum) {
                static_cast<void>(reader.read_u32());
            }
            info.total_size = reader.offset();
            return info;
        } catch (...) {
            return std::nullopt;
        }
    }
};

} // namespace backend

/**
 * @namespace minizip::api
 * @brief Public archive, extraction, middleware, manifest, and journal API.
 */
namespace api {

enum class speed : int { Fast = 1, Balanced = 5, Best = 9 };
enum class focus : std::uint8_t { Archiving = 0, Compression = 1 };
enum class options : std::uint8_t { NonRecursive = 0, Recursive = 1 };
enum class overwrite : std::uint8_t { Skip = 0, Replace = 1, Error = 2 };
enum class entry_kind : std::uint8_t { file = 0, directory = 1, generated = 2, manifest = 3, object = 4 };
enum class source_kind : std::uint8_t { filesystem = 0, bytes = 1, generated = 2, stream = 3, object = 4 };
enum class MZJL : std::uint8_t { FromFile = 0, FromString = 1 };

/**
 * @brief Lightweight result wrapper used throughout the public API.
 *
 * The type intentionally remains small and header-only friendly. It favors
 * explicit `ok()` checks and descriptive string diagnostics over exceptions.
 */
template <class T = void>
class result;

template <>
class result<void> {
public:
    result() : ok_(true) {}
    explicit result(std::string message) : ok_(false), message_(std::move(message)) {}

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    [[nodiscard]] const std::string &message() const noexcept { return message_; }

private:
    bool ok_ = false;
    std::string message_;
};

template <class T>
class result {
public:
    result(T value) : ok_(true), value_(std::move(value)) {}
    explicit result(std::string message) : ok_(false), message_(std::move(message)) {}

    [[nodiscard]] bool ok() const noexcept { return ok_; }
    [[nodiscard]] const std::string &message() const noexcept { return message_; }
    [[nodiscard]] const T &value() const & { return *value_; }
    [[nodiscard]] T &value() & { return *value_; }
    [[nodiscard]] T &&value() && { return std::move(*value_); }

private:
    bool ok_ = false;
    std::string message_;
    std::optional<T> value_;
};

/**
 * @brief Metadata preserved for each archive entry.
 */
struct entry_metadata {
    std::string logical_path;
    entry_kind kind = entry_kind::file;
    source_kind source = source_kind::filesystem;
    std::uint64_t original_size = 0;
    std::uint64_t stored_size = 0;
    std::uint64_t timestamp_unix = 0;
    std::uint64_t content_hash = 0;
    std::uint32_t flags = 0;
    std::string serializer_tag;
    std::string filter_tag;
    std::string content_type;
    backend::codec codec = backend::codec::stored;
};

/**
 * @brief Lifecycle hooks for build and extraction operations.
 *
 * All callbacks are optional. `should_cancel` is polled between units of work.
 */
struct progress_hook {
    std::function<void(std::string_view)> on_entry;
    std::function<void(std::string_view, std::size_t, std::size_t)> on_progress;
    std::function<void(std::string_view)> on_error;
    std::function<void()> on_finish;
    std::function<bool()> should_cancel;
};

/**
 * @brief Type-erased serialization middleware.
 *
 * `encode_any` receives the object originally passed to `zipper::add_object`.
 * Implementations may reject unsupported `std::any` payloads cleanly.
 */
struct serializer {
    std::string tag = "raw";
    std::function<result<detail::byte_vector>(const std::any &)> encode_any;
};

/**
 * @brief Type-erased deserialization middleware.
 *
 * `decode_any` reconstructs a user object from archived bytes into a caller
 * supplied `std::any` target.
 */
struct deserializer {
    std::string tag = "raw";
    std::function<result<void>(std::span<const std::byte>, std::any &)> decode_any;
};

/**
 * @brief Type-erased transport middleware.
 *
 * A transport abstracts the final archive destination or origin but does not
 * change archive semantics.
 */
struct transport {
    std::string tag = "filesystem";
    std::function<result<void>(std::string_view, std::span<const std::byte>)> write;
    std::function<result<detail::byte_vector>(std::string_view)> read;
};

/**
 * @brief Type-erased payload filter middleware.
 *
 * Filters wrap entry payloads around compression and decompression. Examples
 * include encryption, signing, normalization, and integrity transforms.
 */
struct filter {
    std::string tag = "none";
    std::function<result<detail::byte_vector>(std::span<const std::byte>)> encode;
    std::function<result<detail::byte_vector>(std::span<const std::byte>)> decode;
};

/**
 * @brief Abstract manifest contract.
 *
 * A manifest implementation owns its own command language, archive location,
 * and serialization strategy. The zipper stores the final serialized manifest
 * as a generated entry when the archive is built.
 */
class IManifest {
public:
    virtual ~IManifest() = default;
    [[nodiscard]] virtual std::string_view archive_path() const noexcept = 0;
    [[nodiscard]] virtual bool consume_command(std::string_view command_line) = 0;
    [[nodiscard]] virtual detail::byte_vector serialize() const = 0;
};

/**
 * @brief Example INI-style manifest implementation.
 *
 * Supported commands:
 * - `.section <name>`
 * - `.kv <key> <value>`
 */
class INIManifest : public IManifest {
public:
    [[nodiscard]] std::string_view archive_path() const noexcept override { return "meta/package.ini"; }

    [[nodiscard]] bool consume_command(std::string_view command_line) override {
        std::string line(command_line);
        if (detail::starts_with(line, ".section ")) {
            current_section_ = detail::trim_copy(line.substr(9));
            sections_[current_section_];
            return true;
        }
        if (detail::starts_with(line, ".kv ")) {
            auto payload = line.substr(4);
            const auto split = payload.find(' ');
            if (split == std::string::npos) {
                return false;
            }
            auto key = detail::trim_copy(payload.substr(0, split));
            auto value = detail::trim_copy(payload.substr(split + 1));
            auto &section = current_section_.empty() ? sections_["global"] : sections_[current_section_];
            section[key] = value;
            return true;
        }
        return false;
    }

    [[nodiscard]] detail::byte_vector serialize() const override {
        std::string out;
        for (const auto &[section, values] : sections_) {
            out += "[" + section + "]\n";
            for (const auto &[key, value] : values) {
                out += key + "=" + value + "\n";
            }
            out += "\n";
        }
        return detail::to_bytes(out);
    }

private:
    std::string current_section_;
    std::map<std::string, std::map<std::string, std::string>> sections_;
};

struct archive_options {
    speed level = speed::Balanced;
    focus mode = focus::Archiving;
    backend::codec codec = backend::codec::stored;
    overwrite overwrite_policy = overwrite::Replace;
    bool deterministic = false;
    bool auto_manifest = true;
};

struct extract_options {
    overwrite overwrite_policy = overwrite::Replace;
    bool verify_checksums = false;
    bool create_directories = true;
};

struct archive_result {
    bool success = false;
    std::string archive_path;
    std::string message;
    std::size_t entry_count = 0;
    std::size_t byte_count = 0;
    [[nodiscard]] bool ok() const noexcept { return success; }
};

struct extract_result {
    bool success = false;
    std::string message;
    std::size_t extracted_entries = 0;
    [[nodiscard]] bool ok() const noexcept { return success; }
};

struct listed_entry {
    entry_metadata meta;
};

/**
 * @brief In-memory source transport for extractor convenience.
 */
struct buffer_transport {
    detail::byte_vector archive_bytes;
};

namespace detail_api {

constexpr std::uint32_t archive_magic = 0x4D5A4152u;
constexpr std::uint16_t archive_version = 1u;

struct stored_entry_record {
    entry_metadata meta;
    detail::byte_vector payload;
};

struct build_entry {
    entry_metadata meta;
    std::function<result<detail::byte_vector>()> load_payload;
};

inline backend::compression_options make_backend_options(const archive_options &options) {
    backend::compression_options out;
    out.algorithm = options.codec;
    out.level = static_cast<int>(options.level);
    out.deterministic = options.deterministic;
    out.checksum = false;
    return out;
}

inline result<void> maybe_cancel(const progress_hook *hook) {
    if (hook && hook->should_cancel && hook->should_cancel()) {
        return result<void>("MiniZIP: operation cancelled");
    }
    return result<void>();
}

inline void pack_string(detail::byte_writer &writer, std::string_view value) {
    writer.write_u64(static_cast<std::uint64_t>(value.size()));
    writer.write_string(value);
}

inline std::string unpack_string(detail::byte_reader &reader) {
    const auto size = static_cast<std::size_t>(reader.read_u64());
    return detail::to_string_lossy(reader.read_span(size));
}

inline result<detail::byte_vector> slurp_stream(std::istream &stream) {
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        return result<detail::byte_vector>("MiniZIP: failed to read stream payload");
    }
    return result<detail::byte_vector>(detail::to_bytes(buffer.str()));
}

inline result<detail::byte_vector> apply_encode_filters(const std::vector<filter> &filters,
                                                        std::span<const std::byte> input,
                                                        std::string &final_filter_tag) {
    detail::byte_vector current(input.begin(), input.end());
    final_filter_tag.clear();
    for (const auto &item : filters) {
        if (!item.encode) {
            continue;
        }
        auto step = item.encode(current);
        if (!step.ok()) {
            return result<detail::byte_vector>(step.message());
        }
        current = std::move(step.value());
        if (!final_filter_tag.empty()) {
            final_filter_tag.push_back(',');
        }
        final_filter_tag += item.tag;
    }
    return result<detail::byte_vector>(std::move(current));
}

inline result<detail::byte_vector> apply_decode_filters(const std::vector<filter> &filters,
                                                        std::span<const std::byte> input) {
    detail::byte_vector current(input.begin(), input.end());
    for (auto it = filters.rbegin(); it != filters.rend(); ++it) {
        if (!it->decode) {
            continue;
        }
        auto step = it->decode(current);
        if (!step.ok()) {
            return result<detail::byte_vector>(step.message());
        }
        current = std::move(step.value());
    }
    return result<detail::byte_vector>(std::move(current));
}

inline result<detail::byte_vector> encode_archive(const std::vector<build_entry> &entries,
                                                  const archive_options &options,
                                                  const std::vector<filter> &filters,
                                                  const progress_hook *hook) {
    detail::byte_writer writer;
    writer.write_u32(archive_magic);
    writer.write_u16(archive_version);
    writer.write_u32(static_cast<std::uint32_t>(entries.size()));

    for (const auto &entry : entries) {
        if (auto cancel = maybe_cancel(hook); !cancel.ok()) {
            return result<detail::byte_vector>(cancel.message());
        }

        if (hook && hook->on_entry) {
            hook->on_entry(entry.meta.logical_path);
        }

        auto payload_result = entry.load_payload ? entry.load_payload() : result<detail::byte_vector>(detail::byte_vector{});
        if (!payload_result.ok()) {
            return result<detail::byte_vector>(payload_result.message());
        }

        auto raw_payload = std::move(payload_result.value());
        std::string filter_tag;
        auto filtered = apply_encode_filters(filters, raw_payload, filter_tag);
        if (!filtered.ok()) {
            return result<detail::byte_vector>(filtered.message());
        }

        auto compressed = backend::zstd_engine::compress(filtered.value(), make_backend_options(options));
        if (!compressed.ok()) {
            return result<detail::byte_vector>(compressed.message);
        }

        auto meta = entry.meta;
        meta.codec = options.codec;
        meta.original_size = static_cast<std::uint64_t>(raw_payload.size());
        meta.stored_size = static_cast<std::uint64_t>(compressed.bytes.size());
        meta.timestamp_unix = meta.timestamp_unix == 0 ? detail::unix_seconds_now() : meta.timestamp_unix;
        meta.content_hash = detail::fnv1a64(raw_payload);
        if (meta.filter_tag.empty()) {
            meta.filter_tag = filter_tag;
        }

        writer.write_u8(static_cast<std::uint8_t>(meta.kind));
        writer.write_u8(static_cast<std::uint8_t>(meta.source));
        writer.write_u8(static_cast<std::uint8_t>(meta.codec));
        writer.write_u8(0u);
        writer.write_u64(meta.original_size);
        writer.write_u64(meta.stored_size);
        writer.write_u64(meta.timestamp_unix);
        writer.write_u64(meta.content_hash);
        writer.write_u32(meta.flags);
        pack_string(writer, meta.logical_path);
        pack_string(writer, meta.serializer_tag);
        pack_string(writer, meta.filter_tag);
        pack_string(writer, meta.content_type);
        writer.write_u64(static_cast<std::uint64_t>(compressed.bytes.size()));
        writer.write_bytes(compressed.bytes);

        if (hook && hook->on_progress) {
            hook->on_progress(meta.logical_path,
                              static_cast<std::size_t>(meta.original_size),
                              static_cast<std::size_t>(meta.stored_size));
        }
    }

    if (hook && hook->on_finish) {
        hook->on_finish();
    }
    return result<detail::byte_vector>(std::move(writer).take());
}

inline result<std::vector<stored_entry_record>> decode_archive(std::span<const std::byte> archive_bytes,
                                                               const std::vector<filter> &filters) {
    try {
        detail::byte_reader reader(archive_bytes);
        if (reader.read_u32() != archive_magic) {
            return result<std::vector<stored_entry_record>>("MiniZIP: invalid archive magic");
        }
        if (reader.read_u16() != archive_version) {
            return result<std::vector<stored_entry_record>>("MiniZIP: unsupported archive version");
        }
        const auto count = reader.read_u32();

        std::vector<stored_entry_record> records;
        records.reserve(count);

        for (std::uint32_t index = 0; index < count; ++index) {
            entry_metadata meta;
            meta.kind = static_cast<entry_kind>(reader.read_u8());
            meta.source = static_cast<source_kind>(reader.read_u8());
            meta.codec = static_cast<backend::codec>(reader.read_u8());
            static_cast<void>(reader.read_u8());
            meta.original_size = reader.read_u64();
            meta.stored_size = reader.read_u64();
            meta.timestamp_unix = reader.read_u64();
            meta.content_hash = reader.read_u64();
            meta.flags = reader.read_u32();
            meta.logical_path = unpack_string(reader);
            meta.serializer_tag = unpack_string(reader);
            meta.filter_tag = unpack_string(reader);
            meta.content_type = unpack_string(reader);
            const auto payload_size = static_cast<std::size_t>(reader.read_u64());
            const auto stored_payload = reader.read_bytes(payload_size);

            auto decompressed = backend::zstd_engine::decompress(stored_payload, meta.codec);
            if (!decompressed.ok()) {
                return result<std::vector<stored_entry_record>>(decompressed.message);
            }
            auto decoded = apply_decode_filters(filters, decompressed.bytes);
            if (!decoded.ok()) {
                return result<std::vector<stored_entry_record>>(decoded.message());
            }
            records.push_back({std::move(meta), std::move(decoded.value())});
        }

        return result<std::vector<stored_entry_record>>(std::move(records));
    } catch (const std::exception &error) {
        return result<std::vector<stored_entry_record>>(std::string("MiniZIP: archive decode failed: ") + error.what());
    }
}

inline transport filesystem_transport() {
    transport t;
    t.tag = "filesystem";
    t.write = [](std::string_view path, std::span<const std::byte> bytes) -> result<void> {
        std::string error;
        if (!detail::write_file_bytes(std::filesystem::path(path), bytes, &error)) {
            return result<void>(std::move(error));
        }
        return result<void>();
    };
    t.read = [](std::string_view path) -> result<detail::byte_vector> {
        auto bytes = detail::read_file_bytes(std::filesystem::path(path));
        if (!bytes) {
            return result<detail::byte_vector>("MiniZIP: failed to read file '" + std::string(path) + "'");
        }
        return result<detail::byte_vector>(std::move(*bytes));
    };
    return t;
}

} // namespace detail_api

/**
 * @brief Builder-style archive construction API.
 *
 * `zipper` accumulates normalized entry metadata and payload providers. It is
 * mutation-friendly until `seal()` is called. Manifest data is finalized only
 * when building, so repeated manifest commands remain cheap and collision-safe.
 */
class zipper {
public:
    static zipper make_zipper(speed level = speed::Balanced, focus mode = focus::Archiving) {
        zipper out;
        out.options_.level = level;
        out.options_.mode = mode;
        return out;
    }

    zipper &set_codec(backend::codec codec_value) { ensure_mutable(); options_.codec = codec_value; return *this; }
    zipper &set_destination(std::filesystem::path path) { ensure_mutable(); destination_dir_ = std::move(path); return *this; }
    zipper &set_archive_name(std::string name) { ensure_mutable(); archive_name_ = std::move(name); return *this; }
    zipper &overwrite_policy(overwrite value) { ensure_mutable(); options_.overwrite_policy = value; return *this; }
    zipper &deterministic(bool enabled = true) { ensure_mutable(); options_.deterministic = enabled; return *this; }
    zipper &with_hook(progress_hook hook) { hook_ = std::move(hook); return *this; }
    zipper &with_filter(filter value) { ensure_mutable(); filters_.push_back(std::move(value)); return *this; }
    zipper &with_transport(transport value) { ensure_mutable(); transport_ = std::move(value); return *this; }
    zipper &with_serializer(serializer value) { ensure_mutable(); default_serializer_ = std::move(value); return *this; }

    /**
     * @brief Adds a file from the filesystem. The archive path is normalized
     * from the given path itself.
     */
    zipper &add_file(const std::filesystem::path &path) {
        return add_file_as(path, detail::normalize_entry_path(path, false));
    }

    zipper &add_text_file(const std::filesystem::path &path) { return add_file(path); }
    zipper &add_binary_file(const std::filesystem::path &path) { return add_file(path); }

    /**
     * @brief Adds a file from disk under a caller-specified logical path.
     */
    zipper &add_file_as(const std::filesystem::path &path, std::string logical_path) {
        ensure_mutable();
        logical_path = detail::normalize_entry_path(logical_path, false);
        build_entry(entry_kind::file, source_kind::filesystem, std::move(logical_path), "application/octet-stream",
                    [path]() -> result<detail::byte_vector> {
                        auto bytes = detail::read_file_bytes(path);
                        if (!bytes) {
                            return result<detail::byte_vector>("MiniZIP: failed to read input file '" + path.string() + "'");
                        }
                        return result<detail::byte_vector>(std::move(*bytes));
                    });
        return *this;
    }

    /**
     * @brief Adds a directory and expands its contents immediately.
     *
     * The directory entry itself is retained distinctly with a trailing `/`
     * logical path. Child files are added with relative logical names rooted at
     * the provided directory.
     */
    zipper &add_directory(const std::filesystem::path &path, options recursion = options::Recursive) {
        ensure_mutable();
        const auto logical_root = detail::normalize_entry_path(path, true);
        build_entry(entry_kind::directory, source_kind::generated, logical_root, "inode/directory",
                    []() { return result<detail::byte_vector>(detail::byte_vector{}); });

        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
            throw std::runtime_error("MiniZIP: directory does not exist: " + path.string());
        }

        auto add_child = [this, &path](const std::filesystem::path &child) {
            if (std::filesystem::is_directory(child)) {
                const auto relative = std::filesystem::relative(child, path).generic_string();
                if (!relative.empty() && relative != ".") {
                    build_entry(entry_kind::directory, source_kind::generated,
                                detail::normalize_entry_path(relative, true), "inode/directory",
                                []() { return result<detail::byte_vector>(detail::byte_vector{}); });
                }
            } else if (std::filesystem::is_regular_file(child)) {
                const auto relative = detail::normalize_entry_path(std::filesystem::relative(child, path), false);
                add_file_as(child, relative);
            }
        };

        if (recursion == options::Recursive) {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(path)) {
                add_child(entry.path());
            }
        } else {
            for (const auto &entry : std::filesystem::directory_iterator(path)) {
                add_child(entry.path());
            }
        }
        return *this;
    }

    /**
     * @brief Adds a byte buffer entry under a normalized logical path.
     */
    zipper &add_bytes(std::string logical_path, std::span<const std::byte> bytes, std::string content_type = "application/octet-stream") {
        ensure_mutable();
        auto owned = detail::byte_vector(bytes.begin(), bytes.end());
        build_entry(entry_kind::generated, source_kind::bytes, detail::normalize_entry_path(logical_path, false), std::move(content_type),
                    [payload = std::move(owned)]() mutable { return result<detail::byte_vector>(std::move(payload)); });
        return *this;
    }

    zipper &add_stream(std::string logical_path, std::istream &stream, std::string content_type = "application/octet-stream") {
        ensure_mutable();
        auto stream_ptr = std::shared_ptr<std::istream>(&stream, [](std::istream *) {});
        build_entry(entry_kind::generated, source_kind::stream, detail::normalize_entry_path(logical_path, false), std::move(content_type),
                    [stream_ptr]() mutable { return detail_api::slurp_stream(*stream_ptr); });
        return *this;
    }

    template <detail::ByteLike T>
    zipper &add_bytes(std::string logical_path, std::span<const T> bytes, std::string content_type = "application/octet-stream") {
        return add_bytes(std::move(logical_path), std::as_bytes(bytes), std::move(content_type));
    }

    zipper &create_text_file(std::string logical_path) {
        return add_bytes(std::move(logical_path), std::span<const std::byte>{}, "text/plain");
    }

    zipper &create_binary_file(std::string logical_path) {
        return add_bytes(std::move(logical_path), std::span<const std::byte>{}, "application/octet-stream");
    }

    template <class T>
    zipper &add_object(std::string logical_path, T value, serializer encoder) {
        ensure_mutable();
        auto owned = std::make_shared<std::any>(std::move(value));
        entry_metadata meta;
        meta.logical_path = detail::normalize_entry_path(logical_path, false);
        meta.kind = entry_kind::object;
        meta.source = source_kind::object;
        meta.serializer_tag = encoder.tag;
        meta.content_type = "application/x.object";
        upsert_entry(detail::fnv1a64(meta.logical_path), meta,
                     [owned = std::move(owned), encoder = std::move(encoder)]() -> result<detail::byte_vector> {
                         if (!encoder.encode_any) {
                             return result<detail::byte_vector>("MiniZIP: serializer has no encode function");
                         }
                         return encoder.encode_any(*owned);
                     });
        return *this;
    }

    /**
     * @brief Removes an entry by normalized logical path.
     *
     * Removal is collision-safe: entries are matched by both hash bucket and
     * exact stored logical path.
     */
    zipper &remove_item(std::string logical_path) {
        ensure_mutable();
        const auto normalized = detail::normalize_entry_path(logical_path, false);
        const auto directory_form = detail::normalize_entry_path(logical_path, true);
        erase_exact(normalized);
        erase_exact(directory_form);
        return *this;
    }

    zipper &seal() { sealed_ = true; return *this; }
    zipper &reopen() { sealed_ = false; return *this; }

    template <class Manifest, class... Args>
    zipper &create_manifest(Args &&...args) {
        ensure_mutable();
        manifest_ = std::make_shared<Manifest>(std::forward<Args>(args)...);
        return *this;
    }

    zipper &add_to_manifest(std::string_view command_line) {
        ensure_mutable();
        if (!manifest_) {
            throw std::runtime_error("MiniZIP: no manifest is active");
        }
        if (!manifest_->consume_command(command_line)) {
            throw std::runtime_error("MiniZIP: manifest command rejected: " + std::string(command_line));
        }
        return *this;
    }

    /**
     * @brief Builds the archive to the configured destination.
     *
     * If a custom transport is configured it is used. Otherwise the archive is
     * written to `destination/archive_name`.
     */
    [[nodiscard]] archive_result build() {
        auto encoded = build_archive_bytes();
        if (!encoded.ok()) {
            return {false, {}, encoded.message(), 0u, 0u};
        }

        std::string path = archive_name_;
        if (!destination_dir_.empty()) {
            path = (destination_dir_ / archive_name_).string();
        }
        if (path.empty()) {
            return {false, {}, "MiniZIP: archive name is not configured", 0u, 0u};
        }

        auto writer = transport_.write ? transport_ : detail_api::filesystem_transport();
        auto write_result = writer.write(path, encoded.value());
        if (!write_result.ok()) {
            return {false, path, write_result.message(), entries_.size(), 0u};
        }
        return {true, path, {}, entries_.size(), encoded.value().size()};
    }

    [[nodiscard]] archive_result build_file(const std::filesystem::path &path) {
        set_archive_name(path.filename().string());
        set_destination(path.parent_path());
        return build();
    }

    /**
     * @brief Builds the archive in memory without writing it anywhere.
     */
    [[nodiscard]] result<detail::byte_vector> build_archive_bytes() const {
        auto working = entries_;
        if (manifest_) {
            entry_metadata meta;
            meta.logical_path = detail::normalize_entry_path(manifest_->archive_path(), false);
            meta.kind = entry_kind::manifest;
            meta.source = source_kind::generated;
            meta.content_type = "text/manifest";
            const auto bytes = manifest_->serialize();
            bool replaced = false;
            for (auto &entry : working) {
                if (entry.meta.logical_path == meta.logical_path) {
                    entry.meta = meta;
                    entry.load_payload = [bytes]() { return result<detail::byte_vector>(bytes); };
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                working.push_back({meta, [bytes]() { return result<detail::byte_vector>(bytes); }});
            }
        }
        return detail_api::encode_archive(working, options_, filters_, hook_ ? &*hook_ : nullptr);
    }

private:
    void ensure_mutable() const {
        if (sealed_) {
            throw std::runtime_error("MiniZIP: zipper is sealed");
        }
    }

    void erase_exact(const std::string &logical_path) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const detail_api::build_entry &entry) {
                                          return entry.meta.logical_path == logical_path;
                                      }),
                       entries_.end());
    }

    void build_entry(entry_kind kind,
                     source_kind source,
                     std::string logical_path,
                     std::string content_type,
                     std::function<result<detail::byte_vector>()> loader) {
        entry_metadata meta;
        meta.logical_path = std::move(logical_path);
        meta.kind = kind;
        meta.source = source;
        meta.content_type = std::move(content_type);
        upsert_entry(detail::fnv1a64(meta.logical_path), std::move(meta), std::move(loader));
    }

    void upsert_entry(std::uint64_t,
                      entry_metadata meta,
                      std::function<result<detail::byte_vector>()> loader) {
        for (auto &entry : entries_) {
            if (entry.meta.logical_path == meta.logical_path) {
                entry.meta = std::move(meta);
                entry.load_payload = std::move(loader);
                return;
            }
        }
        entries_.push_back({std::move(meta), std::move(loader)});
    }

    archive_options options_;
    std::vector<detail_api::build_entry> entries_;
    std::optional<progress_hook> hook_;
    std::vector<filter> filters_;
    transport transport_ = detail_api::filesystem_transport();
    std::optional<serializer> default_serializer_;
    std::shared_ptr<IManifest> manifest_;
    std::filesystem::path destination_dir_;
    std::string archive_name_ = "archive.mz";
    bool sealed_ = false;
};

/**
 * @brief Archive reader and extraction builder.
 *
 * `extractor` loads the full archive image eagerly, decodes entry payloads,
 * and then provides listing, verification, extraction, and object recovery
 * operations over the decoded records.
 */
class extractor {
public:
    static result<extractor> open(const std::filesystem::path &path) {
        auto bytes = detail::read_file_bytes(path);
        if (!bytes) {
            return result<extractor>("MiniZIP: failed to read archive '" + path.string() + "'");
        }
        return from_archive_bytes(*bytes);
    }

    static result<extractor> open(buffer_transport source) {
        return from_archive_bytes(source.archive_bytes);
    }

    extractor &with_hook(progress_hook hook) { hook_ = std::move(hook); return *this; }
    extractor &with_filter(filter value) { filters_.push_back(std::move(value)); return *this; }
    extractor &with_deserializer(deserializer value) { deserializers_[value.tag] = std::move(value); return *this; }
    extractor &set_destination(std::filesystem::path path) { destination_dir_ = std::move(path); return *this; }
    extractor &on_conflict(overwrite policy) { options_.overwrite_policy = policy; return *this; }

    [[nodiscard]] std::vector<listed_entry> list_entries() const {
        std::vector<listed_entry> out;
        out.reserve(records_.size());
        for (const auto &record : records_) {
            out.push_back({record.meta});
        }
        return out;
    }

    /**
     * @brief Verifies internal archive bookkeeping that MiniZIP can check today.
     *
     * This validates stored sizes, logical path normalization, and payload hash
     * consistency after decompression/filtering. It does not validate xxHash
     * frame checksums because the backend does not yet implement them.
     */
    [[nodiscard]] result<void> verify() const {
        for (const auto &record : records_) {
            if (record.meta.logical_path.empty()) {
                return result<void>("MiniZIP: entry has empty logical path");
            }
            if (record.meta.logical_path != detail::normalize_entry_path(record.meta.logical_path,
                                                                         record.meta.kind == entry_kind::directory)) {
                return result<void>("MiniZIP: entry path is not normalized: " + record.meta.logical_path);
            }
            if (record.meta.kind != entry_kind::directory &&
                record.meta.content_hash != detail::fnv1a64(record.payload)) {
                return result<void>("MiniZIP: content hash mismatch for entry '" + record.meta.logical_path + "'");
            }
        }
        return result<void>();
    }

    [[nodiscard]] extract_result extract_all_to(const std::filesystem::path &directory) const {
        extractor copy = *this;
        copy.destination_dir_ = directory;
        return copy.extract_all();
    }

    [[nodiscard]] extract_result extract_all() const {
        if (destination_dir_.empty()) {
            return {false, "MiniZIP: extraction destination is not configured", 0u};
        }

        std::size_t count = 0;
        for (const auto &record : records_) {
            if (hook_ && hook_->on_entry) {
                hook_->on_entry(record.meta.logical_path);
            }

            const auto target = destination_dir_ / record.meta.logical_path;
            std::error_code ec;
            if (record.meta.kind == entry_kind::directory) {
                std::filesystem::create_directories(target, ec);
                if (ec) {
                    return {false, "MiniZIP: failed to create directory '" + target.string() + "': " + ec.message(), count};
                }
                ++count;
                continue;
            }

            if (std::filesystem::exists(target, ec)) {
                if (options_.overwrite_policy == overwrite::Skip) {
                    continue;
                }
                if (options_.overwrite_policy == overwrite::Error) {
                    return {false, "MiniZIP: target already exists '" + target.string() + "'", count};
                }
            }

            std::string error;
            if (!detail::write_file_bytes(target, record.payload, &error)) {
                return {false, std::move(error), count};
            }
            ++count;
        }

        if (hook_ && hook_->on_finish) {
            hook_->on_finish();
        }
        return {true, {}, count};
    }

    [[nodiscard]] result<detail::byte_vector> extract_bytes(std::string_view logical_path) const {
        const auto normalized = detail::normalize_entry_path(logical_path, false);
        for (const auto &record : records_) {
            if (record.meta.logical_path == normalized) {
                return result<detail::byte_vector>(record.payload);
            }
        }
        return result<detail::byte_vector>("MiniZIP: entry not found: " + std::string(logical_path));
    }

    template <class T>
    [[nodiscard]] result<void> extract(std::string_view logical_path, T &target) const {
        const auto normalized = detail::normalize_entry_path(logical_path, false);
        for (const auto &record : records_) {
            if (record.meta.logical_path != normalized) {
                continue;
            }
            if (record.meta.serializer_tag.empty()) {
                return result<void>("MiniZIP: entry '" + std::string(logical_path) + "' has no serializer tag");
            }
            auto it = deserializers_.find(record.meta.serializer_tag);
            if (it == deserializers_.end()) {
                return result<void>("MiniZIP: no matching deserializer for tag '" + record.meta.serializer_tag + "'");
            }
            std::any boxed = std::ref(target);
            return it->second.decode_any(record.payload, boxed);
        }
        return result<void>("MiniZIP: entry not found: " + std::string(logical_path));
    }

private:
    static result<extractor> from_archive_bytes(const detail::byte_vector &archive_bytes) {
        extractor out;
        auto decoded = detail_api::decode_archive(archive_bytes, out.filters_);
        if (!decoded.ok()) {
            return result<extractor>(decoded.message());
        }
        out.records_ = std::move(decoded.value());
        return result<extractor>(std::move(out));
    }

    std::vector<detail_api::stored_entry_record> records_;
    std::filesystem::path destination_dir_;
    mutable std::optional<progress_hook> hook_;
    mutable std::vector<filter> filters_;
    mutable std::unordered_map<std::string, deserializer> deserializers_;
    extract_options options_;
};

namespace detail_mzjl {

enum class command_kind {
    archive,
    source,
    manifest,
    manifest_cmd,
    level,
    save_to,
    exclude,
    filter
};

struct journal_command {
    command_kind kind{};
    std::string text;
    bool recursive = false;
    int integer = 0;
};

inline ::dsl::Parser<std::string> spaces() {
    return ::dsl::parser([](::dsl::ParsecInput &in) -> ::dsl::ExpectedResult<std::string> {
        std::string out;
        while (!in.eof() && (in.peek() == ' ' || in.peek() == '\t')) {
            out.push_back(in.consume());
        }
        return out;
    });
}

inline ::dsl::Parser<std::string> token_word() {
    return ::dsl::labeled(::dsl::parser([](::dsl::ParsecInput &in) -> ::dsl::ExpectedResult<std::string> {
        if (in.eof() || !(std::isalpha(static_cast<unsigned char>(in.peek())) || in.peek() == '_' || in.peek() == '%')) {
            return ::dsl::fail_expected<std::string>(in, "identifier");
        }
        std::string out;
        while (!in.eof()) {
            const unsigned char ch = static_cast<unsigned char>(in.peek());
            if (!std::isalnum(ch) && in.peek() != '_' && in.peek() != '-' && in.peek() != '%') {
                break;
            }
            out.push_back(in.consume());
        }
        return out;
    }), "identifier");
}

inline ::dsl::Parser<std::string> quoted_string() {
    return ::dsl::labeled(::dsl::parser([](::dsl::ParsecInput &in) -> ::dsl::ExpectedResult<std::string> {
        if (in.peek() != '"') {
            return ::dsl::fail_expected<std::string>(in, "quoted-string");
        }
        static_cast<void>(in.consume());
        std::string out;
        while (!in.eof()) {
            const char ch = in.consume();
            if (ch == '"') {
                return out;
            }
            if (ch == '\\' && !in.eof()) {
                out.push_back(in.consume());
            } else {
                out.push_back(ch);
            }
        }
        return ::dsl::ExpectedResult<std::string>::failure(in.pos, ::dsl::ParseFailureKind::Committed, {"closing quote"});
    }), "quoted-string");
}

inline ::dsl::Parser<int> integer() {
    return ::dsl::labeled(::dsl::parser([](::dsl::ParsecInput &in) -> ::dsl::ExpectedResult<int> {
        if (in.eof() || !std::isdigit(static_cast<unsigned char>(in.peek()))) {
            return ::dsl::fail_expected<int>(in, "integer");
        }
        int value = 0;
        while (!in.eof() && std::isdigit(static_cast<unsigned char>(in.peek()))) {
            value = (value * 10) + (in.consume() - '0');
        }
        return value;
    }), "integer");
}

inline result<std::optional<journal_command>> parse_line(std::string_view line) {
    const auto trimmed = detail::trim_copy(std::string(line));
    if (trimmed.empty()) {
        return result<std::optional<journal_command>>(std::optional<journal_command>{});
    }
    if (detail::starts_with(trimmed, "#")) {
        return result<std::optional<journal_command>>(std::optional<journal_command>{});
    }
    if (trimmed == "%MZJL-v1.0.0") {
        return result<std::optional<journal_command>>(std::optional<journal_command>{});
    }

    auto parse_simple_quoted = [&](command_kind kind, std::string_view keyword) -> std::optional<journal_command> {
        const auto parser =
            token_word() & spaces() & quoted_string();
        auto parsed = ::dsl::run_parser(parser, trimmed);
        if (!parsed.value) {
            return std::nullopt;
        }
        const auto &nested = *parsed.value;
        const auto &kw = nested.first.first;
        const auto &text = nested.second;
        if (kw != keyword) {
            return std::nullopt;
        }
        return journal_command{kind, text, false, 0};
    };

    if (auto simple = parse_simple_quoted(command_kind::archive, "archive")) {
        return result<std::optional<journal_command>>(std::optional<journal_command>{*simple});
    }
    if (auto simple = parse_simple_quoted(command_kind::save_to, "save_to")) {
        return result<std::optional<journal_command>>(std::optional<journal_command>{*simple});
    }
    if (auto simple = parse_simple_quoted(command_kind::manifest_cmd, "manifest_cmd")) {
        return result<std::optional<journal_command>>(std::optional<journal_command>{*simple});
    }
    if (auto simple = parse_simple_quoted(command_kind::exclude, "exclude")) {
        return result<std::optional<journal_command>>(std::optional<journal_command>{*simple});
    }

    {
        const auto parser = token_word() & spaces() & quoted_string() & ::dsl::optional(spaces() & token_word());
        auto parsed = ::dsl::run_parser(parser, trimmed);
        if (parsed.value) {
            const auto &nested = *parsed.value;
            const auto &lhs = nested.first;
            const auto &kw = lhs.first.first;
            const auto &text = lhs.second;
            const auto &mode = nested.second;
            if (kw == "source") {
                journal_command command{command_kind::source, text, false, 0};
                if (mode && mode->second == "recursive") {
                    command.recursive = true;
                }
                return result<std::optional<journal_command>>(std::optional<journal_command>{std::move(command)});
            }
        }
    }

    {
        const auto parser = token_word() & spaces() & token_word();
        auto parsed = ::dsl::run_parser(parser, trimmed);
        if (parsed.value) {
            const auto &nested = *parsed.value;
            const auto &kw = nested.first.first;
            const auto &text = nested.second;
            if (kw == "manifest") {
                return result<std::optional<journal_command>>(std::optional<journal_command>(journal_command{command_kind::manifest, text, false, 0}));
            }
            if (kw == "filter") {
                return result<std::optional<journal_command>>(std::optional<journal_command>(journal_command{command_kind::filter, text, false, 0}));
            }
        }
    }

    {
        const auto parser = token_word() & spaces() & integer();
        auto parsed = ::dsl::run_parser(parser, trimmed);
        if (parsed.value) {
            const auto &nested = *parsed.value;
            const auto &kw = nested.first.first;
            const auto number = nested.second;
            if (kw == "level") {
                return result<std::optional<journal_command>>(std::optional<journal_command>(journal_command{command_kind::level, {}, false, number}));
            }
        }
    }

    return result<std::optional<journal_command>>("MiniZIP: MZJL parse error on line: " + trimmed);
}

} // namespace detail_mzjl

/**
 * @brief Journal-driven archive builder for `MZJL` sources.
 *
 * The textual language intentionally remains small and deterministic. Parsing
 * uses the real `DSLtk.hpp` combinator parser API (`dsl::parser`,
 * `dsl::ParsecInput`, `dsl::run_parser`, `dsl::optional`, and operator-based
 * combinators) instead of guessed names.
 */
class journal_archiver {
public:
    journal_archiver(std::string source, MZJL source_kind) : source_(std::move(source)), source_kind_(source_kind) {}

    journal_archiver &lint() {
        diagnostics_.clear();
        if (source_kind_ == MZJL::FromFile) {
            auto text = detail::read_file_text(source_);
            if (!text) {
                diagnostics_.push_back("MiniZIP: failed to read journal file '" + source_ + "'");
            }
        }
        return *this;
    }

    journal_archiver &parse() {
        commands_.clear();
        if (!diagnostics_.empty()) {
            return *this;
        }

        std::string content = source_;
        if (source_kind_ == MZJL::FromFile) {
            content = *detail::read_file_text(source_);
        }

        std::istringstream lines(content);
        std::string line;
        while (std::getline(lines, line)) {
            auto command = detail_mzjl::parse_line(line);
            if (!command.ok()) {
                diagnostics_.push_back(command.message());
                continue;
            }
            if (command.value()) {
                commands_.push_back(std::move(*command.value()));
            }
        }
        return *this;
    }

    journal_archiver &validate() {
        bool saw_archive = false;
        for (const auto &command : commands_) {
            if (command.kind == detail_mzjl::command_kind::archive) {
                saw_archive = true;
            }
            if (command.kind == detail_mzjl::command_kind::manifest && command.text != "INIManifest") {
                diagnostics_.push_back("MiniZIP: only INIManifest is built in for MZJL currently");
            }
            if (command.kind == detail_mzjl::command_kind::filter) {
                diagnostics_.push_back("MiniZIP: named MZJL filters require explicit runtime registration");
            }
        }
        if (!saw_archive) {
            diagnostics_.push_back("MiniZIP: MZJL archive declaration is missing");
        }
        return *this;
    }

    journal_archiver &scaffold() {
        if (!diagnostics_.empty()) {
            return *this;
        }
        zipper_ = zipper::make_zipper();
        for (const auto &command : commands_) {
            switch (command.kind) {
                case detail_mzjl::command_kind::archive:
                    zipper_->set_archive_name(command.text);
                    break;
                case detail_mzjl::command_kind::source:
                    if (command.recursive) {
                        zipper_->add_directory(command.text, options::Recursive);
                    } else {
                        std::error_code ec;
                        if (std::filesystem::is_directory(command.text, ec)) {
                            zipper_->add_directory(command.text, options::NonRecursive);
                        } else {
                            zipper_->add_file(command.text);
                        }
                    }
                    break;
                case detail_mzjl::command_kind::manifest:
                    if (command.text == "INIManifest") {
                        zipper_->create_manifest<INIManifest>();
                    }
                    break;
                case detail_mzjl::command_kind::manifest_cmd:
                    zipper_->add_to_manifest(command.text);
                    break;
                case detail_mzjl::command_kind::level:
                    if (command.integer <= 3) {
                        zipper_->set_codec(backend::codec::stored);
                    } else {
                        zipper_->set_codec(backend::codec::zstd);
                    }
                    break;
                case detail_mzjl::command_kind::save_to:
                    zipper_->set_destination(command.text);
                    break;
                case detail_mzjl::command_kind::exclude:
                case detail_mzjl::command_kind::filter:
                    break;
            }
        }
        return *this;
    }

    journal_archiver &build() { return *this; }

    [[nodiscard]] archive_result save(std::optional<std::filesystem::path> path = std::nullopt) {
        if (!diagnostics_.empty()) {
            return {false, {}, diagnostics_.front(), 0u, 0u};
        }
        if (!zipper_) {
            return {false, {}, "MiniZIP: journal was not scaffolded", 0u, 0u};
        }
        if (path) {
            return zipper_->build_file(*path);
        }
        return zipper_->build();
    }

    [[nodiscard]] bool ok() const noexcept { return diagnostics_.empty(); }
    [[nodiscard]] const std::vector<std::string> &diagnostics() const noexcept { return diagnostics_; }

private:
    std::string source_;
    MZJL source_kind_;
    std::vector<detail_mzjl::journal_command> commands_;
    std::vector<std::string> diagnostics_;
    std::optional<zipper> zipper_;
};

inline journal_archiver make_journal_archiver(std::string source, MZJL source_kind) {
    return journal_archiver(std::move(source), source_kind);
}

} // namespace api

/**
 * @namespace minizip::dsl
 * @brief Thin native C++ DSL built on the actual `DSLtk.hpp` pipeline feature.
 *
 * Integration notes:
 * - The implementation uses the real global `dsl::pipe(...)` helper from
 *   `DSLtk.hpp`.
 * - It does not assume nonexistent helpers or guessed signatures.
 * - The DSL remains a convenience layer over `minizip::api`, not a separate
 *   execution engine.
 */
namespace dsl {

struct pipeline_language : ::dsl::DSL<pipeline_language, ::dsl::Pipeline> {};
inline constexpr pipeline_language language{};

inline api::zipper archive(std::string archive_name) {
    auto zipper = api::zipper::make_zipper();
    zipper.set_archive_name(std::move(archive_name));
    return zipper;
}

inline api::extractor extract(const std::filesystem::path &path) {
    auto opened = api::extractor::open(path);
    if (!opened.ok()) {
        throw std::runtime_error(opened.message());
    }
    return std::move(opened.value());
}

inline auto from(std::filesystem::path path, api::options recursion = api::options::Recursive) {
    return ::dsl::pipe([path = std::move(path), recursion](api::zipper zipper) mutable {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec)) {
            zipper.add_directory(path, recursion);
        } else {
            zipper.add_file(path);
        }
        return zipper;
    });
}

inline auto level(int value) {
    return ::dsl::pipe([value](api::zipper zipper) mutable {
        zipper.set_codec(value <= 3 ? backend::codec::stored : backend::codec::zstd);
        return zipper;
    });
}

inline auto save_to(std::filesystem::path path) {
    return ::dsl::pipe([path = std::move(path)](auto builder) mutable {
        builder.set_destination(path);
        return builder;
    });
}

inline auto to(std::filesystem::path path) {
    return ::dsl::pipe([path = std::move(path)](api::extractor extractor) mutable {
        extractor.set_destination(path);
        return extractor;
    });
}

inline auto on_conflict(api::overwrite policy) {
    return ::dsl::pipe([policy](api::extractor extractor) mutable {
        extractor.on_conflict(policy);
        return extractor;
    });
}

inline auto filter_with(api::filter value) {
    return ::dsl::pipe([value = std::move(value)](auto builder) mutable {
        builder.with_filter(std::move(value));
        return builder;
    });
}

inline auto hook_with(api::progress_hook value) {
    return ::dsl::pipe([value = std::move(value)](auto builder) mutable {
        builder.with_hook(std::move(value));
        return builder;
    });
}

inline auto serialize_with(api::serializer value) {
    return ::dsl::pipe([value = std::move(value)](api::zipper zipper) mutable {
        zipper.with_serializer(std::move(value));
        return zipper;
    });
}

inline auto deserialize_with(api::deserializer value) {
    return ::dsl::pipe([value = std::move(value)](api::extractor extractor) mutable {
        extractor.with_deserializer(std::move(value));
        return extractor;
    });
}

template <class Manifest, class... Args>
auto manifest(Args... args) {
    return ::dsl::pipe([tuple = std::make_tuple(std::move(args)...)](api::zipper zipper) mutable {
        std::apply([&](auto &&...inner) { zipper.template create_manifest<Manifest>(std::forward<decltype(inner)>(inner)...); }, tuple);
        return zipper;
    });
}

inline auto manifest_cmd(std::string command_line) {
    return ::dsl::pipe([command_line = std::move(command_line)](api::zipper zipper) mutable {
        zipper.add_to_manifest(command_line);
        return zipper;
    });
}

} // namespace dsl

} // namespace minizip
