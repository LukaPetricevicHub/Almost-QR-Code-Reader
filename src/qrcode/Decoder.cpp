#include "Decoder.hpp"

#include "Codewords.hpp"
#include "DataReader.hpp"
#include "FormatInformation.hpp"
#include "ReedSolomon.hpp"
#include "Segments.hpp"

#include <string_view>
#include <utility>
#include <variant>

namespace qrcode {

std::string_view toString(const DecodeError& error) {
    return std::visit(
        [](const auto specificError) {
            return qrcode::toString(specificError);
        },
        error);
}

std::expected<DecodeResult, DecodeError> Decoder::decode(
    const ZXing::BitMatrix& bitmap, QrVersion version) const {
    const auto format = FormatInformationReader::read(bitmap);
    if (!format.has_value()) {
        return std::unexpected(DecodeError{format.error()});
    }

    auto rawBits = DataReader{bitmap, version, format->mask}.readBits();
    const auto layout =
        qrBlockLayout(version, format->errorCorrectionLevel);
    constexpr int bitsPerCodeword = 8;
    const auto codewordBitCount = static_cast<std::size_t>(
        layout.totalCodewords() * bitsPerCodeword);
    const auto expectedRemainderBits =
        static_cast<std::size_t>(version.number() == 1 ? 0 : 7);
    if (rawBits.size() != codewordBitCount + expectedRemainderBits) {
        return std::unexpected(
            DecodeError{CodewordError::invalidBitCount});
    }

    const auto packed =
        packCodewords(std::string_view{rawBits}.substr(0, codewordBitCount));
    if (!packed.has_value()) {
        return std::unexpected(DecodeError{packed.error()});
    }

    auto correctedCodewords = std::move(*packed);
    auto correctedErrors = 0;

    if (version.number() == 1) {
        auto corrected = ReedSolomon::correct(
            correctedCodewords, layout.errorCorrectionCodewordsPerBlock);
        if (!corrected.has_value()) {
            return std::unexpected(DecodeError{corrected.error()});
        }
        correctedCodewords = std::move(corrected->codewords);
        correctedErrors = corrected->correctedErrors;
    }

    const auto dataCodewords =
        deinterleaveDataCodewords(correctedCodewords, layout);
    if (!dataCodewords.has_value()) {
        return std::unexpected(DecodeError{dataCodewords.error()});
    }
    auto dataBits = unpackCodewords(*dataCodewords);

    auto segments = Segments::decodeMessage(dataBits, version);
    if (!segments.has_value()) {
        return std::unexpected(DecodeError{segments.error()});
    }

    return DecodeResult{
        .version = version.number(),
        .mask = format->mask,
        .errorCorrectionLevel = format->errorCorrectionLevel,
        .correctedFormatBits = format->correctedBits,
        .correctedErrors = correctedErrors,
        .bits = std::move(dataBits),
        .segments = std::move(*segments),
    };
}

}  // namespace qrcode
