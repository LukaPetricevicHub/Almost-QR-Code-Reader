#include "Bitstream.hpp"
#include "Codewords.hpp"
#include "DataReader.hpp"
#include "Decoder.hpp"
#include "FormatInformation.hpp"
#include "GaloisField256.hpp"
#include "Masks.hpp"
#include "MessageFormatter.hpp"
#include "QrVersion.hpp"
#include "ReedSolomon.hpp"
#include "Segments.hpp"
#include "TextEncoding.hpp"

#include <BitMatrix.h>
#include <HybridBinarizer.h>
#include <ImageView.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace {

static_assert(qrcode::qrKanjiToShiftJis(0x073F) == 0x8ABF);
static_assert(qrcode::qrKanjiToShiftJis(0x1740) == 0xE040);
static_assert(!qrcode::qrKanjiToShiftJis(0x003F).has_value());
static_assert(qrcode::versionOneBlockLayout(
                  qrcode::ErrorCorrectionLevel::low)
                  .totalCodewords() == 26);
static_assert(qrcode::versionOneBlockLayout(
                  qrcode::ErrorCorrectionLevel::medium)
                  .totalCodewords() == 26);
static_assert(qrcode::versionOneBlockLayout(
                  qrcode::ErrorCorrectionLevel::quartile)
                  .totalCodewords() == 26);
static_assert(qrcode::versionOneBlockLayout(
                  qrcode::ErrorCorrectionLevel::high)
                  .totalCodewords() == 26);

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

ZXing::BitMatrix loadQrBitmap(std::string_view filename) {
    const std::string path =
        std::string{QRCODE_SOURCE_DIR} + "/" + std::string{filename};
    int width = 0;
    int height = 0;
    int channels = 0;
    std::unique_ptr<stbi_uc, void (*)(void*)> buffer(
        stbi_load(path.c_str(), &width, &height, &channels, 0),
        stbi_image_free);
    if (!buffer) {
        throw std::runtime_error("Could not load QR test image");
    }

    constexpr auto formats =
        std::array{ZXing::ImageFormat::None, ZXing::ImageFormat::Lum,
                   ZXing::ImageFormat::LumA, ZXing::ImageFormat::RGB,
                   ZXing::ImageFormat::RGBA};
    if (channels < 1 || channels >= static_cast<int>(formats.size())) {
        throw std::runtime_error("Unsupported test image channel count");
    }

    const auto version = qrcode::QrVersion::fromImageSize(width, height);
    if (!version.has_value()) {
        throw std::runtime_error("Invalid QR test image dimensions");
    }

    const ZXing::ImageView image{buffer.get(), width, height,
                                 formats.at(channels)};
    const int size = version->symbolSize();
    const auto cropped = image.cropped(
        qrcode::QrVersion::quietZoneWidth,
        qrcode::QrVersion::quietZoneWidth, size, size);
    auto bitmap =
        std::make_unique<ZXing::HybridBinarizer>(cropped)->getBlackMatrix();
    return bitmap->copy();
}

struct Coordinate {
    int x = -1;
    int y = -1;
};

std::vector<Coordinate> findDataCoordinates(
    const ZXing::BitMatrix& bitmap, qrcode::QrVersion version, int mask) {
    const auto originalBits =
        qrcode::DataReader{bitmap, version, mask}.readBits();
    std::vector<Coordinate> coordinates(originalBits.size());

    for (int y = 0; y < bitmap.height(); ++y) {
        for (int x = 0; x < bitmap.width(); ++x) {
            auto changed = bitmap.copy();
            changed.flip(x, y);
            const auto changedBits =
                qrcode::DataReader{changed, version, mask}.readBits();

            auto differingIndex = -1;
            auto differenceCount = 0;
            for (std::size_t index = 0; index < originalBits.size(); ++index) {
                if (originalBits.at(index) != changedBits.at(index)) {
                    differingIndex = static_cast<int>(index);
                    ++differenceCount;
                }
            }
            if (differenceCount == 1) {
                coordinates.at(differingIndex) = {.x = x, .y = y};
            }
        }
    }

    for (const auto coordinate : coordinates) {
        expect(coordinate.x >= 0 && coordinate.y >= 0,
               "Every QR data bit should map to one module");
    }
    return coordinates;
}

std::vector<std::uint8_t> makeGeneratorPolynomial(
    int errorCorrectionCodewords) {
    std::vector<std::uint8_t> generator{1};

    for (int degree = 0; degree < errorCorrectionCodewords; ++degree) {
        std::vector<std::uint8_t> next(generator.size() + 1, 0);
        const auto root = qrcode::GaloisField256::exponent(degree);
        for (std::size_t index = 0; index < generator.size(); ++index) {
            next.at(index) ^= generator.at(index);
            next.at(index + 1) ^= qrcode::GaloisField256::multiply(
                generator.at(index), root);
        }
        generator = std::move(next);
    }
    return generator;
}

std::vector<std::uint8_t> encodeTestBlock(
    std::span<const std::uint8_t> data, int errorCorrectionCodewords) {
    const auto generator =
        makeGeneratorPolynomial(errorCorrectionCodewords);
    std::vector<std::uint8_t> remainder(data.begin(), data.end());
    remainder.resize(data.size() + errorCorrectionCodewords, 0);

    for (std::size_t offset = 0; offset < data.size(); ++offset) {
        const auto factor = remainder.at(offset);
        if (factor == 0) {
            continue;
        }
        for (std::size_t index = 0; index < generator.size(); ++index) {
            remainder.at(offset + index) ^=
                qrcode::GaloisField256::multiply(generator.at(index), factor);
        }
    }

    std::vector<std::uint8_t> codewords(data.begin(), data.end());
    codewords.insert(codewords.end(), remainder.end() - errorCorrectionCodewords,
                     remainder.end());
    return codewords;
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

void testFormatInformation() {
    const auto mediumMaskSix =
        qrcode::FormatInformationReader::decode(0x4F97, 0x4F97);
    expect(mediumMaskSix.has_value(),
           "Valid format information should decode");
    expect(mediumMaskSix->errorCorrectionLevel ==
               qrcode::ErrorCorrectionLevel::medium,
           "Format error-correction level mismatch");
    expect(mediumMaskSix->mask == 6, "Format mask mismatch");
    expect(mediumMaskSix->correctedBits == 0,
           "Clean format information should need no correction");

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
    expect(corrected->mask == 6, "Corrected format mask mismatch");
    expect(corrected->correctedBits == 3,
           "Format correction count mismatch");

    const auto secondCopy = qrcode::FormatInformationReader::decode(
        0, lowMaskSix);
    expect(secondCopy.has_value(),
           "An intact second format copy should be sufficient");
    expect(secondCopy->mask == 6, "Second format copy mask mismatch");

    const auto highMaskZero =
        qrcode::FormatInformationReader::decode(0x1689, 0x1689);
    expect(highMaskZero.has_value() &&
               highMaskZero->errorCorrectionLevel ==
                   qrcode::ErrorCorrectionLevel::high &&
               highMaskZero->mask == 0,
           "High-level format mapping mismatch");

    const auto quartileMaskZero =
        qrcode::FormatInformationReader::decode(0x355F, 0x355F);
    expect(quartileMaskZero.has_value() &&
               quartileMaskZero->errorCorrectionLevel ==
                   qrcode::ErrorCorrectionLevel::quartile &&
               quartileMaskZero->mask == 0,
           "Quartile-level format mapping mismatch");

    const auto invalid = qrcode::FormatInformationReader::decode(0, 0);
    expect(!invalid.has_value(),
           "Uncorrectable format information should fail");
    expect(invalid.error() == qrcode::FormatError::uncorrectable,
           "Wrong uncorrectable-format error");
}

void testCodewordPacking() {
    const auto packed =
        qrcode::packCodewords("010000011111111100000000");
    expect(packed.has_value(), "Valid bits should pack into codewords");
    expect(*packed == std::vector<std::uint8_t>{0x41, 0xFF, 0x00},
           "Packed codewords mismatch");
    expect(qrcode::unpackCodewords(*packed) ==
               "010000011111111100000000",
           "Unpacked codewords mismatch");

    const auto partial = qrcode::packCodewords("101");
    expect(!partial.has_value(), "Partial codeword should fail");
    expect(partial.error() == qrcode::CodewordError::invalidBitCount,
           "Wrong partial-codeword error");

    const auto invalid = qrcode::packCodewords("0000000x");
    expect(!invalid.has_value(), "Invalid bit should fail");
    expect(invalid.error() == qrcode::CodewordError::invalidBit,
           "Wrong invalid-bit error");
}

void testGaloisFieldArithmetic() {
    expect(qrcode::GaloisField256::exponent(0) == 1,
           "GF exponent zero mismatch");
    expect(qrcode::GaloisField256::exponent(8) == 0x1D,
           "GF primitive polynomial reduction mismatch");
    expect(qrcode::GaloisField256::add(0x53, 0xCA) == (0x53 ^ 0xCA),
           "GF addition must be XOR");
    expect(qrcode::GaloisField256::multiply(0, 0xA5) == 0,
           "GF zero multiplication mismatch");
    expect(!qrcode::GaloisField256::inverse(0).has_value(),
           "GF zero must not have an inverse");
    expect(!qrcode::GaloisField256::divide(1, 0).has_value(),
           "GF division by zero should fail");

    for (int value = 1; value < 256; ++value) {
        const auto byte = static_cast<std::uint8_t>(value);
        const auto inverse = qrcode::GaloisField256::inverse(byte);
        expect(inverse.has_value(), "Every nonzero GF value needs an inverse");
        expect(qrcode::GaloisField256::multiply(byte, *inverse) == 1,
               "GF multiplicative inverse mismatch");
    }
}

void testReedSolomonIsoVector() {
    const std::vector<std::uint8_t> expected{
        0x10, 0x20, 0x0C, 0x56, 0x61, 0x80, 0xEC, 0x11, 0xEC,
        0x11, 0xEC, 0x11, 0xEC, 0x11, 0xEC, 0x11, 0xA5, 0x24,
        0xD4, 0xC1, 0xED, 0x36, 0xC7, 0x87, 0x2C, 0x55,
    };

    const auto clean = qrcode::ReedSolomon::correct(expected, 10);
    expect(clean.has_value(), "Clean ISO Reed-Solomon vector should pass");
    expect(clean->codewords == expected,
           "Clean Reed-Solomon vector should remain unchanged");
    expect(clean->correctedErrors == 0,
           "Clean Reed-Solomon vector correction count mismatch");

    auto damaged = expected;
    constexpr std::array positions{0, 4, 9, 17, 25};
    for (std::size_t index = 0; index < positions.size(); ++index) {
        damaged.at(positions.at(index)) ^=
            static_cast<std::uint8_t>(0x31 + index);
    }

    const auto corrected = qrcode::ReedSolomon::correct(damaged, 10);
    expect(corrected.has_value(),
           "Five damaged Version 1-M codewords should be corrected");
    expect(corrected->codewords == expected,
           "Corrected ISO Reed-Solomon vector mismatch");
    expect(corrected->correctedErrors == 5,
           "Reed-Solomon correction count mismatch");

    damaged.at(12) ^= 0xA7;
    const auto beyondCapacity =
        qrcode::ReedSolomon::correct(damaged, 10);
    expect(!beyondCapacity.has_value(),
           "Selected damage beyond the correction capacity should fail");
    expect(beyondCapacity.error() == qrcode::ReedSolomonError::uncorrectable,
           "Wrong uncorrectable Reed-Solomon error");
}

void testAllVersionOneCorrectionLevels() {
    constexpr std::array levels{
        qrcode::ErrorCorrectionLevel::low,
        qrcode::ErrorCorrectionLevel::medium,
        qrcode::ErrorCorrectionLevel::quartile,
        qrcode::ErrorCorrectionLevel::high,
    };

    for (std::size_t levelIndex = 0; levelIndex < levels.size();
         ++levelIndex) {
        const auto layout =
            qrcode::versionOneBlockLayout(levels.at(levelIndex));
        std::vector<std::uint8_t> data(layout.dataCodewords);
        for (std::size_t index = 0; index < data.size(); ++index) {
            data.at(index) = static_cast<std::uint8_t>(
                index * 29 + levelIndex * 47 + 3);
        }

        const auto expected =
            encodeTestBlock(data, layout.errorCorrectionCodewords);
        auto damaged = expected;
        const int correctable = layout.errorCorrectionCodewords / 2;
        for (int error = 0; error < correctable; ++error) {
            const auto position =
                static_cast<std::size_t>((error * 5) % expected.size());
            damaged.at(position) ^= static_cast<std::uint8_t>(0x81 + error);
        }

        const auto corrected = qrcode::ReedSolomon::correct(
            damaged, layout.errorCorrectionCodewords);
        expect(corrected.has_value(),
               "Version 1 correction level failed within its capacity");
        expect(corrected->codewords == expected,
               "Version 1 corrected block mismatch");
        expect(corrected->correctedErrors == correctable,
               "Version 1 correction-level count mismatch");
    }
}

void testReedSolomonInputErrors() {
    const std::vector<std::uint8_t> empty;
    const auto noCodewords = qrcode::ReedSolomon::correct(empty, 7);
    expect(!noCodewords.has_value(), "Empty Reed-Solomon block should fail");
    expect(noCodewords.error() ==
               qrcode::ReedSolomonError::invalidCodewordCount,
           "Wrong empty Reed-Solomon block error");

    const std::vector<std::uint8_t> block(26, 0);
    const auto noParity = qrcode::ReedSolomon::correct(block, 0);
    expect(!noParity.has_value(), "Zero parity codewords should fail");
    expect(noParity.error() ==
               qrcode::ReedSolomonError::invalidErrorCorrectionCount,
           "Wrong invalid parity count error");
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

void testVersionOneModuleErrorCorrection() {
    const auto version = versionOne();
    const auto pristine = loadQrBitmap("qr01.png");
    const auto clean = qrcode::Decoder{}.decode(pristine, version);
    expect(clean.has_value(), "Clean Version 1 image should decode");
    expect(qrcode::MessageFormatter::format(clean->segments) == "12345",
           "Clean Version 1 image message mismatch");
    expect(clean->correctedErrors == 0,
           "Clean Version 1 image should need no correction");

    const auto layout =
        qrcode::versionOneBlockLayout(clean->errorCorrectionLevel);
    const int correctable = layout.errorCorrectionCodewords / 2;
    const auto coordinates =
        findDataCoordinates(pristine, version, clean->mask);

    auto damaged = pristine.copy();
    for (int error = 0; error < correctable; ++error) {
        const int codeword =
            error == correctable - 1 ? layout.totalCodewords() - 1 : error;
        const auto coordinate = coordinates.at(codeword * 8);
        damaged.flip(coordinate.x, coordinate.y);
    }

    const auto recovered = qrcode::Decoder{}.decode(damaged, version);
    expect(recovered.has_value(),
           "Damaged Version 1 image should be corrected");
    expect(qrcode::MessageFormatter::format(recovered->segments) == "12345",
           "Corrected Version 1 image message mismatch");
    expect(recovered->correctedErrors == correctable,
           "Corrected Version 1 image error count mismatch");

    auto damagedFormat = pristine.copy();
    constexpr std::array firstCopyCoordinates{
        Coordinate{.x = 8, .y = 0},
        Coordinate{.x = 8, .y = 8},
        Coordinate{.x = 0, .y = 8},
    };
    const int size = damagedFormat.width();
    const std::array secondCopyCoordinates{
        Coordinate{.x = size - 1, .y = 8},
        Coordinate{.x = size - 8, .y = 8},
        Coordinate{.x = 8, .y = size - 1},
    };
    for (std::size_t index = 0; index < firstCopyCoordinates.size();
         ++index) {
        damagedFormat.flip(firstCopyCoordinates.at(index).x,
                           firstCopyCoordinates.at(index).y);
        damagedFormat.flip(secondCopyCoordinates.at(index).x,
                           secondCopyCoordinates.at(index).y);
    }

    const auto recoveredFormat =
        qrcode::Decoder{}.decode(damagedFormat, version);
    expect(recoveredFormat.has_value(),
           "Three damaged bits in both format copies should be corrected");
    expect(qrcode::MessageFormatter::format(recoveredFormat->segments) ==
               "12345",
           "Format-corrected Version 1 image message mismatch");
    expect(recoveredFormat->correctedFormatBits == 3,
           "Image format correction count mismatch");
}

}  // namespace

int main() {
    try {
        testBitstreamReadsSequentialBits();
        testMaskPatterns();
        testFormatInformation();
        testCodewordPacking();
        testGaloisFieldArithmetic();
        testReedSolomonIsoVector();
        testAllVersionOneCorrectionLevels();
        testReedSolomonInputErrors();
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
        testVersionOneModuleErrorCorrection();
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }

    std::println("All tests passed :)");
}
