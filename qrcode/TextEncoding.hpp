#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

namespace qrcode {

enum class TextEncodingError {
    converterUnavailable,
    invalidInput,
};

[[nodiscard]] constexpr std::optional<std::uint16_t> qrKanjiToShiftJis(
    int encoded) {
    if (encoded < 0 || encoded > 0x1FFF) {
        return std::nullopt;
    }

    const int intermediate =
        ((encoded / 0x0C0) << 8) | (encoded % 0x0C0);
    const int shiftJis =
        intermediate < 0x01F00 ? intermediate + 0x08140
                               : intermediate + 0x0C140;

    const bool inQrKanjiRange =
        (shiftJis >= 0x8140 && shiftJis <= 0x9FFC) ||
        (shiftJis >= 0xE040 && shiftJis <= 0xEBBF);
    const int trailingByte = shiftJis & 0xFF;
    const bool validTrailingByte =
        trailingByte >= 0x40 && trailingByte <= 0xFC &&
        trailingByte != 0x7F;

    if (!inQrKanjiRange || !validTrailingByte) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(shiftJis);
}

[[nodiscard]] std::expected<std::string, TextEncodingError> shiftJisToUtf8(
    std::span<const std::uint8_t> bytes);

}  // namespace qrcode
