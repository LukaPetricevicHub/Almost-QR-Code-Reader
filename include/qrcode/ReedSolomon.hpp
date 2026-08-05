#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace qrcode {

enum class ReedSolomonError {
    invalidCodewordCount,
    invalidErrorCorrectionCount,
    uncorrectable,
};

[[nodiscard]] constexpr std::string_view toString(ReedSolomonError error) {
    switch (error) {
        case ReedSolomonError::invalidCodewordCount:
            return "invalid Reed-Solomon codeword count";
        case ReedSolomonError::invalidErrorCorrectionCount:
            return "invalid Reed-Solomon error-correction count";
        case ReedSolomonError::uncorrectable:
            return "QR code contains too many damaged codewords";
    }
    return "unknown Reed-Solomon error";
}

struct ReedSolomonResult {
    std::vector<std::uint8_t> codewords;
    int correctedErrors = 0;
};

class ReedSolomon {
public:
    [[nodiscard]] static std::expected<ReedSolomonResult, ReedSolomonError>
    correct(std::span<const std::uint8_t> codewords,
            int errorCorrectionCodewords);
};

}  // namespace qrcode
