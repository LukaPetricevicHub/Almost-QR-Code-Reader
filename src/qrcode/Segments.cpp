#include "Segments.hpp"

#include "Bitstream.hpp"
#include "TextEncoding.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace qrcode {
namespace {

constexpr std::string_view alphanumericAlphabet =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

std::optional<SegmentMode> segmentModeFromBits(int bits) {
    switch (bits) {
        case 0x0:
            return SegmentMode::terminator;
        case 0x1:
            return SegmentMode::numeric;
        case 0x2:
            return SegmentMode::alphanumeric;
        case 0x4:
            return SegmentMode::byte;
        case 0x8:
            return SegmentMode::kanji;
        default:
            return std::nullopt;
    }
}

}  // namespace

std::string_view toString(SegmentError error) {
    switch (error) {
        case SegmentError::truncatedMode:
            return "truncated mode indicator";
        case SegmentError::unsupportedMode:
            return "unsupported segment mode";
        case SegmentError::truncatedLength:
            return "truncated character count";
        case SegmentError::truncatedPayload:
            return "truncated segment payload";
        case SegmentError::invalidValue:
            return "invalid encoded value";
        case SegmentError::invalidKanji:
            return "invalid QR Kanji value";
        case SegmentError::encodingConversionFailed:
            return "Shift-JIS to UTF-8 conversion failed";
    }
    return "unknown segment error";
}

SegmentResult Segments::decodeMessage(const std::string& bits,
                                      QrVersion version) {
    Bitstream stream{bits};
    std::vector<DecodedSegment> segments;

    while (stream.canRead(4)) {
        const auto modeBits = stream.readInt(4);
        if (!modeBits.has_value()) {
            return std::unexpected(SegmentError::truncatedMode);
        }

        const auto mode = segmentModeFromBits(*modeBits);
        if (!mode.has_value()) {
            return std::unexpected(SegmentError::unsupportedMode);
        }
        if (*mode == SegmentMode::terminator) {
            return segments;
        }

        PayloadResult payload = std::unexpected(SegmentError::unsupportedMode);
        switch (*mode) {
            case SegmentMode::numeric:
                payload = decodeNumeric(stream, version);
                break;
            case SegmentMode::alphanumeric:
                payload = decodeAlphanumeric(stream, version);
                break;
            case SegmentMode::byte:
                payload = decodeByte(stream, version);
                break;
            case SegmentMode::kanji:
                payload = decodeKanji(stream, version);
                break;
            case SegmentMode::terminator:
                break;
        }

        if (!payload.has_value()) {
            return std::unexpected(payload.error());
        }
        segments.push_back(DecodedSegment{
            .mode = *mode,
            .data = std::move(*payload),
        });
    }

    return segments;
}

Segments::PayloadResult Segments::decodeNumeric(Bitstream& stream,
                                                QrVersion version) {
    const auto length =
        stream.readInt(characterCountBits(SegmentMode::numeric, version));
    if (!length.has_value()) {
        return std::unexpected(SegmentError::truncatedLength);
    }

    const int groupsOfThree = *length / 3;
    const int remainingDigits = *length % 3;
    auto neededBits = groupsOfThree * 10;

    if (remainingDigits == 2) {
        neededBits += 7;
    } else if (remainingDigits == 1) {
        neededBits += 4;
    }
    if (!stream.canRead(neededBits)) {
        return std::unexpected(SegmentError::truncatedPayload);
    }

    std::string data;
    data.reserve(*length);

    for (int group = 0; group < groupsOfThree; ++group) {
        const auto value = stream.readInt(10);
        if (!value.has_value()) {
            return std::unexpected(SegmentError::truncatedPayload);
        }
        if (*value > 999) {
            return std::unexpected(SegmentError::invalidValue);
        }

        data.push_back(static_cast<char>('0' + *value / 100));
        data.push_back(static_cast<char>('0' + *value / 10 % 10));
        data.push_back(static_cast<char>('0' + *value % 10));
    }

    if (remainingDigits == 2) {
        const auto value = stream.readInt(7);
        if (!value.has_value()) {
            return std::unexpected(SegmentError::truncatedPayload);
        }
        if (*value > 99) {
            return std::unexpected(SegmentError::invalidValue);
        }
        data.push_back(static_cast<char>('0' + *value / 10));
        data.push_back(static_cast<char>('0' + *value % 10));
    } else if (remainingDigits == 1) {
        const auto value = stream.readInt(4);
        if (!value.has_value()) {
            return std::unexpected(SegmentError::truncatedPayload);
        }
        if (*value > 9) {
            return std::unexpected(SegmentError::invalidValue);
        }
        data.push_back(static_cast<char>('0' + *value));
    }

    return data;
}

Segments::PayloadResult Segments::decodeAlphanumeric(Bitstream& stream,
                                                     QrVersion version) {
    const auto length =
        stream.readInt(characterCountBits(SegmentMode::alphanumeric, version));
    if (!length.has_value()) {
        return std::unexpected(SegmentError::truncatedLength);
    }

    const int pairs = *length / 2;
    const int remainingChars = *length % 2;
    const int neededBits = pairs * 11 + remainingChars * 6;
    if (!stream.canRead(neededBits)) {
        return std::unexpected(SegmentError::truncatedPayload);
    }

    std::string data;
    data.reserve(*length);

    for (int pair = 0; pair < pairs; ++pair) {
        const auto value = stream.readInt(11);
        if (!value.has_value()) {
            return std::unexpected(SegmentError::truncatedPayload);
        }
        if (*value >= 45 * 45) {
            return std::unexpected(SegmentError::invalidValue);
        }
        data.push_back(alphanumericAlphabet.at(*value / 45));
        data.push_back(alphanumericAlphabet.at(*value % 45));
    }

    if (remainingChars == 1) {
        const auto value = stream.readInt(6);
        if (!value.has_value()) {
            return std::unexpected(SegmentError::truncatedPayload);
        }
        if (*value >= 45) {
            return std::unexpected(SegmentError::invalidValue);
        }
        data.push_back(alphanumericAlphabet.at(*value));
    }

    return data;
}

Segments::PayloadResult Segments::decodeByte(Bitstream& stream,
                                             QrVersion version) {
    const auto length =
        stream.readInt(characterCountBits(SegmentMode::byte, version));
    if (!length.has_value()) {
        return std::unexpected(SegmentError::truncatedLength);
    }
    if (!stream.canRead(*length * 8)) {
        return std::unexpected(SegmentError::truncatedPayload);
    }

    std::string data;
    data.reserve(*length);

    for (int i = 0; i < *length; ++i) {
        const auto value = stream.readInt(8);
        if (!value.has_value()) {
            return std::unexpected(SegmentError::truncatedPayload);
        }
        data.push_back(static_cast<char>(*value));
    }

    return data;
}

Segments::PayloadResult Segments::decodeKanji(Bitstream& stream,
                                              QrVersion version) {
    const auto length =
        stream.readInt(characterCountBits(SegmentMode::kanji, version));
    if (!length.has_value()) {
        return std::unexpected(SegmentError::truncatedLength);
    }
    if (!stream.canRead(*length * 13)) {
        return std::unexpected(SegmentError::truncatedPayload);
    }

    std::vector<std::uint8_t> shiftJisBytes;
    shiftJisBytes.reserve(*length * 2);

    for (int i = 0; i < *length; ++i) {
        const auto encoded = stream.readInt(13);
        if (!encoded.has_value()) {
            return std::unexpected(SegmentError::truncatedPayload);
        }

        const auto shiftJis = qrKanjiToShiftJis(*encoded);
        if (!shiftJis.has_value()) {
            return std::unexpected(SegmentError::invalidKanji);
        }
        shiftJisBytes.push_back(static_cast<std::uint8_t>(*shiftJis >> 8));
        shiftJisBytes.push_back(static_cast<std::uint8_t>(*shiftJis & 0xFF));
    }

    auto utf8 = shiftJisToUtf8(std::span<const std::uint8_t>{shiftJisBytes});
    if (!utf8.has_value()) {
        return std::unexpected(SegmentError::encodingConversionFailed);
    }
    return std::move(*utf8);
}

}  // namespace qrcode
