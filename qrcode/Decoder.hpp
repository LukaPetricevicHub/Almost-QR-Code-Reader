#pragma once

#include <BitMatrix.h>

#include "QrVersion.hpp"
#include "Segments.hpp"

#include <expected>
#include <string>
#include <vector>

namespace qrcode {

struct DecodeResult {
    int version = 0;
    int mask = 0;
    std::string bits;
    std::vector<DecodedSegment> segments;
};

class Decoder {
public:
    [[nodiscard]] std::expected<DecodeResult, SegmentError> decode(
        const ZXing::BitMatrix& bitmap, QrVersion version) const;
};

}  // namespace qrcode
