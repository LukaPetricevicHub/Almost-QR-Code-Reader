#pragma once

#include <BitMatrix.h>

#include <cstdint>
#include <expected>
#include <string_view>

namespace qrcode {

enum class ErrorCorrectionLevel {
    low,
    medium,
    quartile,
    high,
};

[[nodiscard]] constexpr std::string_view toString(
    ErrorCorrectionLevel level) {
    switch (level) {
        case ErrorCorrectionLevel::low:
            return "L";
        case ErrorCorrectionLevel::medium:
            return "M";
        case ErrorCorrectionLevel::quartile:
            return "Q";
        case ErrorCorrectionLevel::high:
            return "H";
    }
    return "?";
}

enum class FormatError {
    invalidBitmap,
    uncorrectable,
};

[[nodiscard]] constexpr std::string_view toString(FormatError error) {
    switch (error) {
        case FormatError::invalidBitmap:
            return "invalid QR bitmap for format information";
        case FormatError::uncorrectable:
            return "uncorrectable QR format information";
    }
    return "unknown format information error";
}

struct FormatInformation {
    ErrorCorrectionLevel errorCorrectionLevel;
    int mask = 0;
    int correctedBits = 0;
};

class FormatInformationReader {
public:
    [[nodiscard]] static std::expected<FormatInformation, FormatError> read(
        const ZXing::BitMatrix& bitmap);

    [[nodiscard]] static std::expected<FormatInformation, FormatError> decode(
        std::uint16_t firstCopy, std::uint16_t secondCopy);
};

}  // namespace qrcode
