#include "Bitstream.hpp"
#include "DataReader.hpp"
#include "Masks.hpp"
#include "MessageFormatter.hpp"
#include "QrVersion.hpp"
#include "Segments.hpp"
#include "TextEncoding.hpp"

#include <BitMatrix.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

static_assert(qrcode::qrKanjiToShiftJis(0x073F) == 0x8ABF);
static_assert(qrcode::qrKanjiToShiftJis(0x1740) == 0xE040);
static_assert(!qrcode::qrKanjiToShiftJis(0x003F).has_value());

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

std::vector<qrcode::DecodedSegment> decodeSegments(const std::string& bits) {
    auto result = qrcode::Segments::decodeMessage(bits, versionOne());
    if (!result.has_value()) {
        throw std::runtime_error(std::string{qrcode::toString(result.error())});
    }
    return std::move(*result);
}

std::string decodeForDisplay(const std::string& bits) {
    const auto segments = decodeSegments(bits);
    return qrcode::MessageFormatter::format(segments);
}

void testBitstreamReadsSequentialBits() {
    const std::string bits = "101100";
    qrcode::Bitstream stream{bits};

    expect(stream.canRead(3), "Bitstream should contain three bits");
    expect(stream.readInt(3) == 5, "First three bits should equal five");
    expect(stream.readInt(2) == 2, "Next two bits should equal two");
    expect(!stream.canRead(2), "Only one bit should remain");
    expect(!stream.readInt(2).has_value(), "Oversized read should fail");
    expect(stream.readInt(1) == 0, "Last bit should equal zero");
}

void testMaskPatterns() {
    expect(!qrcode::Masks::applies(0, 2, 3), "Mask 0 mismatch");
    expect(!qrcode::Masks::applies(1, 2, 3), "Mask 1 mismatch");
    expect(!qrcode::Masks::applies(2, 2, 3), "Mask 2 mismatch");
    expect(!qrcode::Masks::applies(3, 2, 3), "Mask 3 mismatch");
    expect(!qrcode::Masks::applies(4, 2, 3), "Mask 4 mismatch");
    expect(qrcode::Masks::applies(5, 2, 3), "Mask 5 mismatch");
    expect(qrcode::Masks::applies(6, 2, 3), "Mask 6 mismatch");
    expect(!qrcode::Masks::applies(7, 2, 3), "Mask 7 mismatch");

    expect(qrcode::Masks::applies(0, 4, 6), "Second mask 0 mismatch");
    expect(qrcode::Masks::applies(1, 4, 6), "Second mask 1 mismatch");
    expect(!qrcode::Masks::applies(2, 4, 6), "Second mask 2 mismatch");
    expect(!qrcode::Masks::applies(3, 4, 6), "Second mask 3 mismatch");
}

void testNumericSegment() {
    std::string bits;
    appendBits(bits, 0b0001, 4);
    appendBits(bits, 5, 10);
    appendBits(bits, 123, 10);
    appendBits(bits, 45, 7);
    appendBits(bits, 0, 4);

    expect(decodeForDisplay(bits) == "12345", "Numeric segment mismatch");
}

void testAlphanumericSegment() {
    std::string bits;
    appendBits(bits, 0b0010, 4);
    appendBits(bits, 5, 9);
    appendBits(bits, 17 * 45 + 14, 11);
    appendBits(bits, 21 * 45 + 21, 11);
    appendBits(bits, 24, 6);
    appendBits(bits, 0, 4);

    expect(decodeForDisplay(bits) == "HELLO",
           "Alphanumeric segment mismatch");
}

void testPrintableByteSegment() {
    std::string bits;
    appendBits(bits, 0b0100, 4);
    appendBits(bits, 3, 8);
    appendBits(bits, 'H', 8);
    appendBits(bits, 'i', 8);
    appendBits(bits, '!', 8);
    appendBits(bits, 0, 4);

    expect(decodeForDisplay(bits) == "Hi!", "Printable byte segment mismatch");
}

void testBinaryByteSegment() {
    constexpr std::array values{0x00, 0x1F, 0x7F, 0x80, 0xFF};
    std::string bits;
    appendBits(bits, 0b0100, 4);
    appendBits(bits, static_cast<int>(values.size()), 8);
    for (const int value : values) {
        appendBits(bits, value, 8);
    }
    appendBits(bits, 0, 4);

    const auto segments = decodeSegments(bits);
    expect(segments.size() == 1, "Expected one binary segment");
    expect(segments.front().mode == qrcode::SegmentMode::byte,
           "Expected byte segment mode");
    expect(segments.front().data.size() == values.size(),
           "Binary byte count mismatch");

    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto actual =
            static_cast<unsigned char>(segments.front().data.at(index));
        expect(actual == values.at(index), "Binary byte value mismatch");
    }

    expect(qrcode::MessageFormatter::format(segments) ==
               "\\x00\\x1F\\x7F\\x80\\xFF",
           "Binary display escaping mismatch");
}

void testKanjiSegment() {
    std::string bits;
    appendBits(bits, 0b1000, 4);
    appendBits(bits, 2, 8);
    appendBits(bits, 0x073F, 13);  // 漢, Shift-JIS 0x8ABF
    appendBits(bits, 0x1740, 13);  // 漾, Shift-JIS 0xE040
    appendBits(bits, 0, 4);

    expect(decodeForDisplay(bits) ==
               "\xE6\xBC\xA2\xE6\xBC\xBE",
           "Kanji segment mismatch");
}

void testMixedSegments() {
    std::string bits;
    appendBits(bits, 0b0010, 4);
    appendBits(bits, 2, 9);
    appendBits(bits, 17 * 45 + 18, 11);  // HI
    appendBits(bits, 0b0100, 4);
    appendBits(bits, 1, 8);
    appendBits(bits, 0, 8);
    appendBits(bits, 0b1000, 4);
    appendBits(bits, 1, 8);
    appendBits(bits, 0x073F, 13);
    appendBits(bits, 0, 4);

    expect(decodeForDisplay(bits) == "HI\\x00\xE6\xBC\xA2",
           "Mixed segment mismatch");
}

void testSegmentErrors() {
    const auto unsupported =
        qrcode::Segments::decodeMessage("11110000", versionOne());
    expect(!unsupported.has_value(), "Unsupported mode should fail");
    expect(unsupported.error() == qrcode::SegmentError::unsupportedMode,
           "Wrong unsupported-mode error");

    std::string truncatedByte;
    appendBits(truncatedByte, 0b0100, 4);
    appendBits(truncatedByte, 2, 8);
    appendBits(truncatedByte, 0x41, 8);
    const auto truncated =
        qrcode::Segments::decodeMessage(truncatedByte, versionOne());
    expect(!truncated.has_value(), "Truncated byte segment should fail");
    expect(truncated.error() == qrcode::SegmentError::truncatedPayload,
           "Wrong truncated-payload error");

    std::string invalidKanji;
    appendBits(invalidKanji, 0b1000, 4);
    appendBits(invalidKanji, 1, 8);
    appendBits(invalidKanji, 0x003F, 13);
    const auto invalid =
        qrcode::Segments::decodeMessage(invalidKanji, versionOne());
    expect(!invalid.has_value(), "Invalid Kanji should fail");
    expect(invalid.error() == qrcode::SegmentError::invalidKanji,
           "Wrong invalid-Kanji error");
}

void testCharacterCountWidths() {
    const auto version = versionOne();
    expect(qrcode::characterCountBits(qrcode::SegmentMode::numeric, version) ==
               10,
           "Numeric count width mismatch");
    expect(qrcode::characterCountBits(
               qrcode::SegmentMode::alphanumeric, version) == 9,
           "Alphanumeric count width mismatch");
    expect(qrcode::characterCountBits(qrcode::SegmentMode::byte, version) == 8,
           "Byte count width mismatch");
    expect(qrcode::characterCountBits(qrcode::SegmentMode::kanji, version) == 8,
           "Kanji count width mismatch");
}

void testSupportedQrVersions() {
    constexpr std::array expectedSymbolSizes{21, 25, 29, 33};
    constexpr std::array expectedImageSizes{29, 33, 37, 41};

    for (int index = 0; index < static_cast<int>(expectedSymbolSizes.size());
         ++index) {
        const auto version = qrcode::QrVersion::fromNumber(index + 1);
        expect(version.has_value(), "Expected supported QR version");
        expect(version->symbolSize() == expectedSymbolSizes.at(index),
               "Symbol size mismatch");
        expect(version->imageSize() == expectedImageSizes.at(index),
               "Image size mismatch");
        expect(qrcode::QrVersion::fromSymbolSize(version->symbolSize()) ==
                   version,
               "Symbol-size version inference mismatch");
        expect(qrcode::QrVersion::fromImageSize(version->imageSize(),
                                                version->imageSize()) == version,
               "Image-size version inference mismatch");
    }

    expect(!qrcode::QrVersion::fromNumber(0).has_value(),
           "Version 0 should be rejected");
    expect(!qrcode::QrVersion::fromNumber(5).has_value(),
           "Version 5 should be rejected");
    expect(!qrcode::QrVersion::fromSymbolSize(27).has_value(),
           "Invalid symbol size should be rejected");
    expect(!qrcode::QrVersion::fromImageSize(33, 34).has_value(),
           "Non-square image should be rejected");
}

void testAlignmentPatternCenters() {
    const auto version1 = qrcode::QrVersion::fromNumber(1);
    const auto version2 = qrcode::QrVersion::fromNumber(2);
    const auto version3 = qrcode::QrVersion::fromNumber(3);
    const auto version4 = qrcode::QrVersion::fromNumber(4);

    expect(version1->alignmentPatternCenters().empty(),
           "Version 1 should have no alignment pattern");
    expect(version2->alignmentPatternCenters().front() == 6,
           "Version 2 first alignment center mismatch");
    expect(version2->alignmentPatternCenters().back() == 18,
           "Version 2 last alignment center mismatch");
    expect(version3->alignmentPatternCenters().back() == 22,
           "Version 3 alignment center mismatch");
    expect(version4->alignmentPatternCenters().back() == 26,
           "Version 4 alignment center mismatch");
}

void testDataModuleCountsForVersionsOneToFour() {
    constexpr std::array<std::size_t, 4> expectedBitCounts{208, 359, 567, 807};

    for (int index = 0; index < static_cast<int>(expectedBitCounts.size());
         ++index) {
        const auto version = qrcode::QrVersion::fromNumber(index + 1);
        ZXing::BitMatrix bitmap{version->symbolSize(), version->symbolSize()};
        const auto bits =
            qrcode::DataReader{bitmap, *version, 0}.readBits();

        expect(bits.size() == expectedBitCounts.at(index),
               "Data-module count mismatch");
    }
}

}  // namespace

int main() {
    try {
        testBitstreamReadsSequentialBits();
        testMaskPatterns();
        testNumericSegment();
        testAlphanumericSegment();
        testPrintableByteSegment();
        testBinaryByteSegment();
        testKanjiSegment();
        testMixedSegments();
        testSegmentErrors();
        testCharacterCountWidths();
        testSupportedQrVersions();
        testAlignmentPatternCenters();
        testDataModuleCountsForVersionsOneToFour();
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }

    std::println("All tests passed :)");
}
