#include "FormatInformation.hpp"
#include "Masks.hpp"
#include "MessageFormatter.hpp"
#include "QrVersion.hpp"
#include "ReedSolomon.hpp"
#include "Segments.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

qrcode::QrVersion versionOne() {
    const auto version = qrcode::QrVersion::fromNumber(1);
    if (!version.has_value()) {
        throw std::runtime_error("Version 1 must be supported");
    }
    return *version;
}

void appendBits(std::string& bits, int value, int width) {
    for (int bit = width - 1; bit >= 0; --bit) {
        bits.push_back(((value >> bit) & 1) == 1 ? '1' : '0');
    }
}

std::string decodeForDisplay(const std::string& bits) {
    const auto decoded =
        qrcode::Segments::decodeMessage(bits, versionOne());
    if (!decoded.has_value()) {
        throw std::runtime_error(
            std::string{qrcode::toString(decoded.error())});
    }
    return qrcode::MessageFormatter::format(*decoded);
}

void testAllMasks() {
    constexpr std::array expectedAtTwoThree{
        false, false, false, false, false, true, true, false,
    };
    constexpr std::array expectedAtFourSix{
        true, true, false, false, true, true, true, true,
    };

    for (int mask = 0; mask < 8; ++mask) {
        expect(qrcode::Masks::applies(mask, 2, 3) ==
                   expectedAtTwoThree.at(mask),
               "Mask pattern mismatch at (2, 3)");
        expect(qrcode::Masks::applies(mask, 4, 6) ==
                   expectedAtFourSix.at(mask),
               "Mask pattern mismatch at (4, 6)");
    }
}

void testFormatInformation() {
    const auto clean =
        qrcode::FormatInformationReader::decode(0x4F97, 0x4F97);
    expect(clean.has_value(), "Valid format information should decode");
    expect(clean->errorCorrectionLevel ==
               qrcode::ErrorCorrectionLevel::medium,
           "Format error-correction level mismatch");
    expect(clean->mask == 6, "Format mask mismatch");
    expect(clean->correctedBits == 0,
           "Clean format information should not need correction");

    constexpr std::uint16_t lowMaskSix = 0x6C41;
    constexpr std::uint16_t threeFlips =
        (1U << 0U) | (1U << 7U) | (1U << 14U);
    const auto corrected = qrcode::FormatInformationReader::decode(
        lowMaskSix ^ threeFlips, lowMaskSix ^ threeFlips);
    expect(corrected.has_value(),
           "Three damaged format bits should be corrected");
    expect(corrected->errorCorrectionLevel ==
               qrcode::ErrorCorrectionLevel::low,
           "Corrected format level mismatch");
    expect(corrected->mask == 6 && corrected->correctedBits == 3,
           "Corrected format information mismatch");

    const auto secondCopy =
        qrcode::FormatInformationReader::decode(0, lowMaskSix);
    expect(secondCopy.has_value() && secondCopy->mask == 6,
           "An intact second format copy should be sufficient");
}

void testNumericAndAlphanumericModes() {
    std::string numeric;
    appendBits(numeric, 0b0001, 4);
    appendBits(numeric, 5, 10);
    appendBits(numeric, 123, 10);
    appendBits(numeric, 45, 7);
    appendBits(numeric, 0, 4);
    expect(decodeForDisplay(numeric) == "12345",
           "Numeric segment mismatch");

    std::string alphanumeric;
    appendBits(alphanumeric, 0b0010, 4);
    appendBits(alphanumeric, 5, 9);
    appendBits(alphanumeric, 17 * 45 + 14, 11);  // HE
    appendBits(alphanumeric, 21 * 45 + 21, 11);  // LL
    appendBits(alphanumeric, 24, 6);              // O
    appendBits(alphanumeric, 0, 4);
    expect(decodeForDisplay(alphanumeric) == "HELLO",
           "Alphanumeric segment mismatch");
}

void testByteAndKanjiModes() {
    std::string bytes;
    appendBits(bytes, 0b0100, 4);
    appendBits(bytes, 3, 8);
    appendBits(bytes, 'H', 8);
    appendBits(bytes, 'i', 8);
    appendBits(bytes, '!', 8);
    appendBits(bytes, 0, 4);
    expect(decodeForDisplay(bytes) == "Hi!", "Byte segment mismatch");

    std::string kanji;
    appendBits(kanji, 0b1000, 4);
    appendBits(kanji, 2, 8);
    appendBits(kanji, 0x073F, 13);  // 漢
    appendBits(kanji, 0x1740, 13);  // 漾
    appendBits(kanji, 0, 4);
    expect(decodeForDisplay(kanji) == "漢漾", "Kanji segment mismatch");
}

void testMalformedSegment() {
    const auto unsupported =
        qrcode::Segments::decodeMessage("11110000", versionOne());
    expect(!unsupported.has_value(),
           "An unsupported mode should be rejected");
    expect(unsupported.error() == qrcode::SegmentError::unsupportedMode,
           "Wrong error for unsupported mode");

    std::string truncatedByte;
    appendBits(truncatedByte, 0b0100, 4);
    appendBits(truncatedByte, 2, 8);
    appendBits(truncatedByte, 'A', 8);
    const auto truncated =
        qrcode::Segments::decodeMessage(truncatedByte, versionOne());
    expect(!truncated.has_value() &&
               truncated.error() == qrcode::SegmentError::truncatedPayload,
           "A truncated byte segment should be rejected");
}

void testSupportedVersions() {
    constexpr std::array symbolSizes{21, 25, 29, 33};
    for (int number = 1; number <= 4; ++number) {
        const auto version = qrcode::QrVersion::fromNumber(number);
        expect(version.has_value(), "Expected a supported QR version");
        expect(version->symbolSize() == symbolSizes.at(number - 1),
               "QR symbol size mismatch");
        expect(qrcode::QrVersion::fromImageSize(
                   version->imageSize(), version->imageSize()) == version,
               "QR image-size inference mismatch");
    }

    expect(!qrcode::QrVersion::fromNumber(0).has_value(),
           "Version 0 should be rejected");
    expect(!qrcode::QrVersion::fromNumber(5).has_value(),
           "Version 5 should be rejected");
}

void testVersionOneErrorCorrection() {
    const std::vector<std::uint8_t> expected{
        0x10, 0x20, 0x0C, 0x56, 0x61, 0x80, 0xEC, 0x11, 0xEC,
        0x11, 0xEC, 0x11, 0xEC, 0x11, 0xEC, 0x11, 0xA5, 0x24,
        0xD4, 0xC1, 0xED, 0x36, 0xC7, 0x87, 0x2C, 0x55,
    };

    auto damaged = expected;
    constexpr std::array positions{0, 4, 9, 17, 25};
    for (std::size_t index = 0; index < positions.size(); ++index) {
        damaged.at(positions.at(index)) ^=
            static_cast<std::uint8_t>(0x31 + index);
    }

    const auto corrected = qrcode::ReedSolomon::correct(damaged, 10);
    expect(corrected.has_value(),
           "Five damaged Version 1 codewords should be corrected");
    expect(corrected->codewords == expected,
           "Corrected Version 1 codewords mismatch");
    expect(corrected->correctedErrors == 5,
           "Version 1 correction count mismatch");

    damaged.at(12) ^= 0xA7;
    const auto uncorrectable =
        qrcode::ReedSolomon::correct(damaged, 10);
    expect(!uncorrectable.has_value(),
           "Damage beyond correction capacity should fail");
}

}  // namespace

int main() {
    try {
        testAllMasks();
        testFormatInformation();
        testNumericAndAlphanumericModes();
        testByteAndKanjiModes();
        testMalformedSegment();
        testSupportedVersions();
        testVersionOneErrorCorrection();
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }

    std::println("All QR unit tests passed :)");
}
