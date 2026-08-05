#pragma once

#include <cstdint>
#include <optional>

namespace qrcode {

class GaloisField256 {
public:
    [[nodiscard]] static constexpr std::uint8_t add(std::uint8_t left,
                                                    std::uint8_t right) {
        return static_cast<std::uint8_t>(left ^ right);
    }

    [[nodiscard]] static std::uint8_t exponent(int power);
    [[nodiscard]] static std::optional<int> logarithm(std::uint8_t value);
    [[nodiscard]] static std::uint8_t multiply(std::uint8_t left,
                                               std::uint8_t right);
    [[nodiscard]] static std::optional<std::uint8_t> inverse(
        std::uint8_t value);
    [[nodiscard]] static std::optional<std::uint8_t> divide(
        std::uint8_t numerator, std::uint8_t denominator);
};

}  // namespace qrcode
