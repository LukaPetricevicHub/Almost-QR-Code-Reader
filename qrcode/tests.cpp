#include "Bitstream.hpp"
#include "DataReader.hpp"
#include "Masks.hpp"
#include "QrVersion.hpp"
#include "Segments.hpp"

#include <BitMatrix.h>

#include <array>
#include <cassert>
#include <print>
#include <string>

namespace {

void appendBits(std::string& bits, int value, int width) {
    for (int bit = width - 1; bit >= 0; --bit) {
        bits.push_back(((value >> bit) & 1) == 1 ? '1' : '0');
    }
}

void testBitstreamReadsSequentialBits() {
    const std::string bits = "101100";
    qrcode::Bitstream stream{bits};

    assert(stream.canRead(3));
    assert(stream.readInt(3) == 5);
    assert(stream.readInt(2) == 2);
    assert(!stream.canRead(2));
    assert(!stream.readInt(2).has_value());
    assert(stream.readInt(1) == 0);
}

void testMaskPatterns() {
    assert(!qrcode::Masks::applies(0, 2, 3));
    assert(!qrcode::Masks::applies(1, 2, 3));
    assert(!qrcode::Masks::applies(2, 2, 3));
    assert(!qrcode::Masks::applies(3, 2, 3));
    assert(!qrcode::Masks::applies(4, 2, 3));
    assert(qrcode::Masks::applies(5, 2, 3));
    assert(qrcode::Masks::applies(6, 2, 3));
    assert(!qrcode::Masks::applies(7, 2, 3));

    assert(qrcode::Masks::applies(0, 4, 6));
    assert(qrcode::Masks::applies(1, 4, 6));
    assert(!qrcode::Masks::applies(2, 4, 6));
    assert(!qrcode::Masks::applies(3, 4, 6));
}

void testNumericSegment() {
    std::string bits;
    appendBits(bits, 0b0001, 4);
    appendBits(bits, 5, 10);
    appendBits(bits, 123, 10);
    appendBits(bits, 45, 7);
    appendBits(bits, 0, 4);

    assert(qrcode::Segments::decodeMessage(bits) == "12345");
}

void testAlphanumericSegment() {
    std::string bits;
    appendBits(bits, 0b0010, 4);
    appendBits(bits, 5, 9);
    appendBits(bits, 17 * 45 + 14, 11);
    appendBits(bits, 21 * 45 + 21, 11);
    appendBits(bits, 24, 6);
    appendBits(bits, 0, 4);

    assert(qrcode::Segments::decodeMessage(bits) == "HELLO");
}

void testByteSegment() {
    std::string bits;
    appendBits(bits, 0b0100, 4);
    appendBits(bits, 3, 8);
    appendBits(bits, 'H', 8);
    appendBits(bits, 'i', 8);
    appendBits(bits, '!', 8);
    appendBits(bits, 0, 4);

    assert(qrcode::Segments::decodeMessage(bits) == "Hi!");
}

void testInvalidSegmentReturnsEmptyOptional() {
    const std::string bits = "11110000";

    assert(!qrcode::Segments::decodeMessage(bits).has_value());
}

void testSupportedQrVersions() {
    constexpr std::array expectedSymbolSizes{21, 25, 29, 33};
    constexpr std::array expectedImageSizes{29, 33, 37, 41};

    for (int index = 0; index < static_cast<int>(expectedSymbolSizes.size()); ++index) {
        const auto version = qrcode::QrVersion::fromNumber(index + 1);
        assert(version.has_value());
        assert(version->symbolSize() == expectedSymbolSizes.at(index));
        assert(version->imageSize() == expectedImageSizes.at(index));
        assert(qrcode::QrVersion::fromSymbolSize(version->symbolSize()) == version);
        assert(qrcode::QrVersion::fromImageSize(version->imageSize(),
                                                version->imageSize()) == version);
    }

    assert(!qrcode::QrVersion::fromNumber(0).has_value());
    assert(!qrcode::QrVersion::fromNumber(5).has_value());
    assert(!qrcode::QrVersion::fromSymbolSize(27).has_value());
    assert(!qrcode::QrVersion::fromImageSize(33, 34).has_value());
}

void testAlignmentPatternCenters() {
    const auto version1 = qrcode::QrVersion::fromNumber(1);
    const auto version2 = qrcode::QrVersion::fromNumber(2);
    const auto version3 = qrcode::QrVersion::fromNumber(3);
    const auto version4 = qrcode::QrVersion::fromNumber(4);

    assert(version1->alignmentPatternCenters().empty());
    assert(version2->alignmentPatternCenters().front() == 6);
    assert(version2->alignmentPatternCenters().back() == 18);
    assert(version3->alignmentPatternCenters().back() == 22);
    assert(version4->alignmentPatternCenters().back() == 26);
}

void testDataModuleCountsForVersionsOneToFour() {
    constexpr std::array<std::size_t, 4> expectedBitCounts{208, 359, 567, 807};

    for (int index = 0; index < static_cast<int>(expectedBitCounts.size()); ++index) {
        const auto version = qrcode::QrVersion::fromNumber(index + 1);
        ZXing::BitMatrix bitmap{version->symbolSize(), version->symbolSize()};
        const auto bits = qrcode::DataReader{bitmap, *version, 0}.readBits();

        assert(bits.size() == expectedBitCounts.at(index));
    }
}

}  // namespace

int main() {
    testBitstreamReadsSequentialBits();
    testMaskPatterns();
    testNumericSegment();
    testAlphanumericSegment();
    testByteSegment();
    testInvalidSegmentReturnsEmptyOptional();
    testSupportedQrVersions();
    testAlignmentPatternCenters();
    testDataModuleCountsForVersionsOneToFour();

    std::println("All tests passed :)");
}
