#include "Masks.hpp"

#include <ranges>

namespace qrcode {

bool Masks::applies(int mask, int x, int y) {
    const auto product = x * y;

    switch (mask) {
        case 0:
            return (x + y) % 2 == 0;
        case 1:
            return y % 2 == 0;
        case 2:
            return x % 3 == 0;
        case 3:
            return (x + y) % 3 == 0;
        case 4:
            return (y / 2 + x / 3) % 2 == 0;
        case 5:
            return (product % 2 + product % 3) == 0;
        case 6:
            return (product % 2 + product % 3) % 2 == 0;
        case 7:
            return ((x + y) % 2 + product % 3) % 2 == 0;
        default:
            return false;
    }
}

int Masks::readFromFormatInformation(const ZXing::BitMatrix& bitmap) {
    constexpr int formatInformationMask = 0b101010000010010;

    auto formatBits = 0;
    auto setFormatBit = [&bitmap, &formatBits](int bitIndex, int x, int y) {
        if (bitmap.get(x, y)) {
            formatBits |= 1 << bitIndex;
        }
    };

    for (auto i : std::views::iota(0, 6)) {
        setFormatBit(i, 8, i);
    }
    setFormatBit(6, 8, 7);
    setFormatBit(7, 8, 8);
    setFormatBit(8, 7, 8);

    for (auto i : std::views::iota(9, 15)) {
        setFormatBit(i, 14 - i, 8);
    }

    const auto unmaskedFormatBits = formatBits ^ formatInformationMask;
    return (unmaskedFormatBits >> 10) & 0b111;
}

}  // namespace qrcode
