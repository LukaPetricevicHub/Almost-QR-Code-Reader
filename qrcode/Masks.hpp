#pragma once

#include <BitMatrix.h>

namespace qrcode {

class Masks {
public:
    [[nodiscard]] static bool applies(int mask, int x, int y);
    [[nodiscard]] static int readFromFormatInformation(const ZXing::BitMatrix& bitmap);
};

}  // namespace qrcode
