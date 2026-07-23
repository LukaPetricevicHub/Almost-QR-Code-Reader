#pragma once

#include <BitMatrix.h>

#include "QrVersion.hpp"

#include <string>

namespace qrcode {

struct DecodeResult {
    int version = 0;
    int mask = 0;
    std::string bits;
    std::string message;
};

class Decoder {
public:
    [[nodiscard]] DecodeResult decode(const ZXing::BitMatrix& bitmap,
                                      QrVersion version) const;
};

}  // namespace qrcode
