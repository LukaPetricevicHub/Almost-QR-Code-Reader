#pragma once

#include <BitMatrix.h>

#include <string>

namespace qrcode {

struct DecodeResult {
    int mask = 0;
    std::string bits;
    std::string message;
};

class Decoder {
public:
    [[nodiscard]] DecodeResult decode(const ZXing::BitMatrix& bitmap, int version) const;
};

}  // namespace qrcode
