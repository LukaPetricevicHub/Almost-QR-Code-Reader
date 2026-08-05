#pragma once

#include <BitMatrix.h>

#include "Codewords.hpp"
#include "FormatInformation.hpp"
#include "QrVersion.hpp"
#include "ReedSolomon.hpp"
#include "Segments.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace qrcode {

using DecodeError = std::variant<FormatError, CodewordError, ReedSolomonError, SegmentError>;

[[nodiscard]] std::string_view toString(const DecodeError& error);

struct DecodeResult {
    int version = 0;
    int mask = 0;
    ErrorCorrectionLevel errorCorrectionLevel =
        ErrorCorrectionLevel::medium;
    int correctedFormatBits = 0;
    int correctedErrors = 0;
    std::string bits;
    std::vector<DecodedSegment> segments;
};

class Decoder {
public:
    [[nodiscard]] std::expected<DecodeResult, DecodeError> decode(
        const ZXing::BitMatrix& bitmap, QrVersion version) const;
};

}  // namespace qrcode
