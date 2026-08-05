#pragma once

#include <optional>
#include <span>

namespace qrcode {

class QrVersion {
public:
    static constexpr int minimum = 1;
    static constexpr int maximum = 4;
    static constexpr int quietZoneWidth = 4;

    [[nodiscard]] static constexpr std::optional<QrVersion> fromNumber(int number) {
        if (number < minimum || number > maximum) {
            return std::nullopt;
        }
        return QrVersion{number};
    }

    [[nodiscard]] static constexpr std::optional<QrVersion> fromSymbolSize(int size) {
        if (size < 21 || (size - 17) % 4 != 0) {
            return std::nullopt;
        }
        return fromNumber((size - 17) / 4);
    }

    [[nodiscard]] static constexpr std::optional<QrVersion> fromImageSize(
        int width, int height) {
        if (width != height) {
            return std::nullopt;
        }
        return fromSymbolSize(width - 2 * quietZoneWidth);
    }

    [[nodiscard]] constexpr int number() const {
        return number_;
    }

    [[nodiscard]] constexpr int symbolSize() const {
        return 17 + 4 * number_;
    }

    [[nodiscard]] constexpr int imageSize() const {
        return symbolSize() + 2 * quietZoneWidth;
    }

    [[nodiscard]] std::span<const int> alignmentPatternCenters() const;

    bool operator==(const QrVersion&) const = default;

private:
    explicit constexpr QrVersion(int number) : number_(number) {}

    int number_;
};

}  // namespace qrcode
