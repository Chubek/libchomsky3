#pragma once

/*
 * zbase64.hpp - Tiny header-only Base64 encoder/decoder
 *
 * Features:
 * - C++17 compatible
 * - Header-only
 * - std::string API
 * - Optional URL-safe mode
 * - No external dependencies
 *
 * Usage:
 *
 *   #include "zbase64.hpp"
 *
 *   std::string encoded = zbase64::encode("hello world");
 *   std::string decoded = zbase64::decode(encoded);
 *
 *   auto url = zbase64::encode("data", true);
 */

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace zbase64 {

namespace detail {

static constexpr char kStandardAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static constexpr char kUrlAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

inline constexpr std::array<int8_t, 256> make_decode_table(bool urlsafe) {
    std::array<int8_t, 256> table{};

    for (auto& v : table)
        v = -1;

    const char* alphabet =
        urlsafe ? kUrlAlphabet : kStandardAlphabet;

    for (int i = 0; i < 64; ++i)
        table[static_cast<uint8_t>(alphabet[i])] =
            static_cast<int8_t>(i);

    return table;
}

static constexpr auto kDecodeTable =
    make_decode_table(false);

static constexpr auto kDecodeTableUrl =
    make_decode_table(true);

} // namespace detail

inline std::string encode(std::string_view input,
                          bool urlsafe = false,
                          bool pad = true) {
    const char* alphabet =
        urlsafe
            ? detail::kUrlAlphabet
            : detail::kStandardAlphabet;

    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;

    while (i + 3 <= input.size()) {
        uint32_t n =
            (static_cast<uint8_t>(input[i]) << 16) |
            (static_cast<uint8_t>(input[i + 1]) << 8) |
            (static_cast<uint8_t>(input[i + 2]));

        out.push_back(alphabet[(n >> 18) & 0x3F]);
        out.push_back(alphabet[(n >> 12) & 0x3F]);
        out.push_back(alphabet[(n >> 6) & 0x3F]);
        out.push_back(alphabet[n & 0x3F]);

        i += 3;
    }

    const size_t remain = input.size() - i;

    if (remain == 1) {
        uint32_t n =
            static_cast<uint8_t>(input[i]) << 16;

        out.push_back(alphabet[(n >> 18) & 0x3F]);
        out.push_back(alphabet[(n >> 12) & 0x3F]);

        if (pad) {
            out.push_back('=');
            out.push_back('=');
        }
    } else if (remain == 2) {
        uint32_t n =
            (static_cast<uint8_t>(input[i]) << 16) |
            (static_cast<uint8_t>(input[i + 1]) << 8);

        out.push_back(alphabet[(n >> 18) & 0x3F]);
        out.push_back(alphabet[(n >> 12) & 0x3F]);
        out.push_back(alphabet[(n >> 6) & 0x3F]);

        if (pad)
            out.push_back('=');
    }

    return out;
}

inline std::string decode(std::string_view input,
                          bool urlsafe = false) {
    const auto& table =
        urlsafe
            ? detail::kDecodeTableUrl
            : detail::kDecodeTable;

    std::vector<uint8_t> bytes;
    bytes.reserve((input.size() * 3) / 4);

    uint32_t buffer = 0;
    int bits = 0;

    for (char c : input) {
        if (c == '=')
            break;

        const int8_t val =
            table[static_cast<uint8_t>(c)];

        if (val < 0) {
            if (c == '\n' || c == '\r' ||
                c == ' ' || c == '\t')
                continue;

            throw std::runtime_error(
                "Invalid Base64 character");
        }

        buffer = (buffer << 6) | val;
        bits += 6;

        if (bits >= 8) {
            bits -= 8;

            bytes.push_back(
                static_cast<uint8_t>(
                    (buffer >> bits) & 0xFF));
        }
    }

    return std::string(bytes.begin(), bytes.end());
}

} // namespace zbase64