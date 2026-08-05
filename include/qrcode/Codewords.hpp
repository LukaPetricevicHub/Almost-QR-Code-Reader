#pragma once

#include "FormatInformation.hpp"
#include "QrVersion.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace qrcode {

enum class CodewordError {
    invalidBitCount,
    invalidBit,
    invalidCodewordCount,
};

[[nodiscard]] constexpr std::string_view toString(CodewordError error) {
    switch (error) {
        case CodewordError::invalidBitCount:
            return "QR bit count is not a whole number of codewords";
        case CodewordError::invalidBit:
            return "QR bit stream contains a value other than zero or one";
        case CodewordError::invalidCodewordCount:
            return "QR codeword count does not match its block layout";
    }
    return "unknown codeword error";
}

struct QrBlockLayout {
    int blockCount;
    int dataCodewordsPerBlock;
    int errorCorrectionCodewordsPerBlock;

    [[nodiscard]] constexpr int totalDataCodewords() const {
        return blockCount * dataCodewordsPerBlock;
    }

    [[nodiscard]] constexpr int totalCodewords() const {
        return blockCount *
               (dataCodewordsPerBlock + errorCorrectionCodewordsPerBlock);
    }
};

[[nodiscard]] constexpr QrBlockLayout qrBlockLayout(
    QrVersion version, ErrorCorrectionLevel level) {
    constexpr std::array<std::array<QrBlockLayout, 4>, 4> layouts{{
        {{
            {1, 19, 7},
            {1, 16, 10},
            {1, 13, 13},
            {1, 9, 17},
        }},
        {{
            {1, 34, 10},
            {1, 28, 16},
            {1, 22, 22},
            {1, 16, 28},
        }},
        {{
            {1, 55, 15},
            {1, 44, 26},
            {2, 17, 18},
            {2, 13, 22},
        }},
        {{
            {1, 80, 20},
            {2, 32, 18},
            {2, 24, 26},
            {4, 9, 16},
        }},
    }};

    return layouts.at(version.number() - 1)
        .at(static_cast<std::size_t>(level));
}

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodewordError>
packCodewords(std::string_view bits);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodewordError>
deinterleaveDataCodewords(std::span<const std::uint8_t> codewords,
                          QrBlockLayout layout);

[[nodiscard]] std::string unpackCodewords(
    std::span<const std::uint8_t> codewords);

}  // namespace qrcode
