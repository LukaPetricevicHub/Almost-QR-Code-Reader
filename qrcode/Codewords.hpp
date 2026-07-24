#pragma once

#include "FormatInformation.hpp"

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
};

[[nodiscard]] constexpr std::string_view toString(CodewordError error) {
    switch (error) {
        case CodewordError::invalidBitCount:
            return "QR bit count is not a whole number of codewords";
        case CodewordError::invalidBit:
            return "QR bit stream contains a value other than zero or one";
    }
    return "unknown codeword error";
}

struct VersionOneBlockLayout {
    int dataCodewords;
    int errorCorrectionCodewords;

    [[nodiscard]] constexpr int totalCodewords() const {
        return dataCodewords + errorCorrectionCodewords;
    }
};

[[nodiscard]] constexpr VersionOneBlockLayout versionOneBlockLayout(
    ErrorCorrectionLevel level) {
    switch (level) {
        case ErrorCorrectionLevel::low:
            return {.dataCodewords = 19, .errorCorrectionCodewords = 7};
        case ErrorCorrectionLevel::medium:
            return {.dataCodewords = 16, .errorCorrectionCodewords = 10};
        case ErrorCorrectionLevel::quartile:
            return {.dataCodewords = 13, .errorCorrectionCodewords = 13};
        case ErrorCorrectionLevel::high:
            return {.dataCodewords = 9, .errorCorrectionCodewords = 17};
    }
    return {};
}

[[nodiscard]] std::expected<std::vector<std::uint8_t>, CodewordError>
packCodewords(std::string_view bits);

[[nodiscard]] std::string unpackCodewords(
    std::span<const std::uint8_t> codewords);

}  // namespace qrcode
