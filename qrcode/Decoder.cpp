#include "Decoder.hpp"

#include "Codewords.hpp"
#include "DataReader.hpp"
#include "FormatInformation.hpp"
#include "ReedSolomon.hpp"
#include "Segments.hpp"

#include <span>
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
    auto dataBits = rawBits;
    auto correctedErrors = 0;

    if (version.number() == 1) {
        const auto layout =
            versionOneBlockLayout(format->errorCorrectionLevel);
        const auto codewords = packCodewords(rawBits);
        if (!codewords.has_value()) {
            return std::unexpected(DecodeError{codewords.error()});
        }
        if (static_cast<int>(codewords->size()) != layout.totalCodewords()) {
            return std::unexpected(
                DecodeError{ReedSolomonError::invalidCodewordCount});
        }

        auto corrected = ReedSolomon::correct(
            *codewords, layout.errorCorrectionCodewords);
        if (!corrected.has_value()) {
            return std::unexpected(DecodeError{corrected.error()});
        }

        const auto dataCodewords = std::span{corrected->codewords}.first(
            static_cast<std::size_t>(layout.dataCodewords));
        dataBits = unpackCodewords(dataCodewords);
        correctedErrors = corrected->correctedErrors;
    }

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
