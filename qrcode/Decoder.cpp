#include "Decoder.hpp"

#include "DataReader.hpp"
#include "Masks.hpp"
#include "Segments.hpp"

#include <utility>

namespace qrcode {

DecodeResult Decoder::decode(const ZXing::BitMatrix& bitmap, int version) const {
    const auto mask = Masks::readFromFormatInformation(bitmap);
    auto bits = DataReader{bitmap, version, mask}.readBits();
    auto message = Segments::decodeMessage(bits).value_or(std::string{});

    return DecodeResult{
        .mask = mask,
        .bits = std::move(bits),
        .message = std::move(message),
    };
}

}  // namespace qrcode
