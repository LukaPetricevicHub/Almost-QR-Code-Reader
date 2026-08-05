#include "FormatInformation.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace qrcode {
namespace {

// ISO/IEC 18004 Model 2 format information after the fixed XOR mask.
constexpr std::array<std::uint16_t, 32> maskedFormatPatterns{
    0x5412, 0x5125, 0x5E7C, 0x5B4B, 0x45F9, 0x40CE, 0x4F97, 0x4AA0,
    0x77C4, 0x72F3, 0x7DAA, 0x789D, 0x662F, 0x6318, 0x6C41, 0x6976,
    0x1689, 0x13BE, 0x1CE7, 0x19D0, 0x0762, 0x0255, 0x0D0C, 0x083B,
    0x355F, 0x3068, 0x3F31, 0x3A06, 0x24B4, 0x2183, 0x2EDA, 0x2BED,
};

constexpr ErrorCorrectionLevel levelFromBits(int bits) {
    constexpr std::array levels{
        ErrorCorrectionLevel::medium,
        ErrorCorrectionLevel::low,
        ErrorCorrectionLevel::high,
        ErrorCorrectionLevel::quartile,
    };
    return levels.at(bits);
}

std::uint16_t readFirstCopy(const ZXing::BitMatrix& bitmap) {
    std::uint16_t bits = 0;
    auto setBit = [&bitmap, &bits](int bitIndex, int x, int y) {
        if (bitmap.get(x, y)) {
            bits |= static_cast<std::uint16_t>(1U << bitIndex);
        }
    };

    for (int index = 0; index < 6; ++index) {
        setBit(index, 8, index);
    }
    setBit(6, 8, 7);
    setBit(7, 8, 8);
    setBit(8, 7, 8);
    for (int index = 9; index < 15; ++index) {
        setBit(index, 14 - index, 8);
    }
    return bits;
}

std::uint16_t readSecondCopy(const ZXing::BitMatrix& bitmap) {
    const int size = bitmap.width();
    std::uint16_t bits = 0;
    auto setBit = [&bitmap, &bits](int bitIndex, int x, int y) {
        if (bitmap.get(x, y)) {
            bits |= static_cast<std::uint16_t>(1U << bitIndex);
        }
    };

    for (int index = 0; index < 8; ++index) {
        setBit(index, size - 1 - index, 8);
    }
    for (int index = 8; index < 15; ++index) {
        setBit(index, 8, size - 15 + index);
    }
    return bits;
}

}  // namespace

std::expected<FormatInformation, FormatError> FormatInformationReader::read(
    const ZXing::BitMatrix& bitmap) {
    if (bitmap.width() != bitmap.height() || bitmap.width() < 21) {
        return std::unexpected(FormatError::invalidBitmap);
    }
    return decode(readFirstCopy(bitmap), readSecondCopy(bitmap));
}

std::expected<FormatInformation, FormatError> FormatInformationReader::decode(
    std::uint16_t firstCopy, std::uint16_t secondCopy) {
    auto bestDistance = std::numeric_limits<int>::max();
    auto bestData = 0;

    for (int data = 0; data < static_cast<int>(maskedFormatPatterns.size());
         ++data) {
        const auto pattern = maskedFormatPatterns.at(data);
        const auto firstDistance =
            std::popcount(static_cast<std::uint16_t>(firstCopy ^ pattern));
        const auto secondDistance =
            std::popcount(static_cast<std::uint16_t>(secondCopy ^ pattern));
        const auto distance = firstDistance < secondDistance
                                  ? firstDistance
                                  : secondDistance;

        if (distance < bestDistance) {
            bestDistance = distance;
            bestData = data;
        }
    }

    if (bestDistance > 3) {
        return std::unexpected(FormatError::uncorrectable);
    }

    return FormatInformation{
        .errorCorrectionLevel = levelFromBits((bestData >> 3) & 0b11),
        .mask = bestData & 0b111,
        .correctedBits = bestDistance,
    };
}

}  // namespace qrcode
