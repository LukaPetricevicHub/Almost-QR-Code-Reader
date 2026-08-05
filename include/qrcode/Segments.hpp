#pragma once

#include "QrVersion.hpp"

#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace qrcode {

class Bitstream;

enum class SegmentMode : int {
    terminator = 0x0,
    numeric = 0x1,
    alphanumeric = 0x2,
    byte = 0x4,
    kanji = 0x8,
};

enum class SegmentError {
    truncatedMode,
    unsupportedMode,
    truncatedLength,
    truncatedPayload,
    invalidValue,
    invalidKanji,
    encodingConversionFailed,
};

struct DecodedSegment {
    SegmentMode mode;
    // Raw bytes for byte mode; UTF-8 text for all text modes.
    std::string data;
};

using SegmentResult =
    std::expected<std::vector<DecodedSegment>, SegmentError>;

[[nodiscard]] constexpr int characterCountBits(SegmentMode mode,
                                                QrVersion version) {
    constexpr std::array numericWidths{10, 12, 14};
    constexpr std::array alphanumericWidths{9, 11, 13};
    constexpr std::array byteWidths{8, 16, 16};
    constexpr std::array kanjiWidths{8, 10, 12};

    const int group = version.number() <= 9
                          ? 0
                          : (version.number() <= 26 ? 1 : 2);

    switch (mode) {
        case SegmentMode::numeric:
            return numericWidths.at(group);
        case SegmentMode::alphanumeric:
            return alphanumericWidths.at(group);
        case SegmentMode::byte:
            return byteWidths.at(group);
        case SegmentMode::kanji:
            return kanjiWidths.at(group);
        case SegmentMode::terminator:
            return 0;
    }
    return 0;
}

[[nodiscard]] std::string_view toString(SegmentError error);

class Segments {
public:
    [[nodiscard]] static SegmentResult decodeMessage(const std::string& bits,
                                                     QrVersion version);

private:
    using PayloadResult = std::expected<std::string, SegmentError>;

    [[nodiscard]] static PayloadResult decodeNumeric(Bitstream& stream,
                                                     QrVersion version);
    [[nodiscard]] static PayloadResult decodeAlphanumeric(Bitstream& stream,
                                                          QrVersion version);
    [[nodiscard]] static PayloadResult decodeByte(Bitstream& stream,
                                                  QrVersion version);
    [[nodiscard]] static PayloadResult decodeKanji(Bitstream& stream,
                                                   QrVersion version);
};

}  // namespace qrcode
