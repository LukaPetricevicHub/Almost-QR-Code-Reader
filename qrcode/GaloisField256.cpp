#include "GaloisField256.hpp"

#include <array>

namespace qrcode {
namespace {

struct FieldTables {
    std::array<std::uint8_t, 512> exponents{};
    std::array<int, 256> logarithms{};
};

constexpr FieldTables makeFieldTables() {
    constexpr int primitivePolynomial = 0x11D;
    FieldTables tables;
    int value = 1;

    for (int power = 0; power < 255; ++power) {
        tables.exponents.at(power) = static_cast<std::uint8_t>(value);
        tables.logarithms.at(value) = power;

        value <<= 1;
        if ((value & 0x100) != 0) {
            value ^= primitivePolynomial;
        }
    }
    for (int power = 255;
         power < static_cast<int>(tables.exponents.size()); ++power) {
        tables.exponents.at(power) = tables.exponents.at(power - 255);
    }
    return tables;
}

constexpr auto fieldTables = makeFieldTables();

}  // namespace

std::uint8_t GaloisField256::exponent(int power) {
    auto normalized = power % 255;
    if (normalized < 0) {
        normalized += 255;
    }
    return fieldTables.exponents.at(normalized);
}

std::optional<int> GaloisField256::logarithm(std::uint8_t value) {
    if (value == 0) {
        return std::nullopt;
    }
    return fieldTables.logarithms.at(value);
}

std::uint8_t GaloisField256::multiply(std::uint8_t left,
                                      std::uint8_t right) {
    if (left == 0 || right == 0) {
        return 0;
    }
    const auto exponent =
        fieldTables.logarithms.at(left) + fieldTables.logarithms.at(right);
    return fieldTables.exponents.at(exponent);
}

std::optional<std::uint8_t> GaloisField256::inverse(std::uint8_t value) {
    if (value == 0) {
        return std::nullopt;
    }
    return fieldTables.exponents.at(255 - fieldTables.logarithms.at(value));
}

std::optional<std::uint8_t> GaloisField256::divide(
    std::uint8_t numerator, std::uint8_t denominator) {
    if (denominator == 0) {
        return std::nullopt;
    }
    if (numerator == 0) {
        return 0;
    }

    auto exponent = fieldTables.logarithms.at(numerator) -
                    fieldTables.logarithms.at(denominator);
    if (exponent < 0) {
        exponent += 255;
    }
    return fieldTables.exponents.at(exponent);
}

}  // namespace qrcode
