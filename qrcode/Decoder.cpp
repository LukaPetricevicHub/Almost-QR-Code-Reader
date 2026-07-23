#include "Decoder.hpp"

#include "DataReader.hpp"
#include "Masks.hpp"
#include "Segments.hpp"

#include <utility>

namespace qrcode {

std::expected<DecodeResult, SegmentError> Decoder::decode(
    const ZXing::BitMatrix& bitmap, QrVersion version) const {
    const auto mask = Masks::readFromFormatInformation(bitmap);
    auto bits = DataReader{bitmap, version, mask}.readBits();
    auto segments = Segments::decodeMessage(bits, version);
    if (!segments.has_value()) {
        return std::unexpected(segments.error());
    }

    return DecodeResult{
        .version = version.number(),
        .mask = mask,
        .bits = std::move(bits),
        .segments = std::move(*segments),
    };
}

}  // namespace qrcode
