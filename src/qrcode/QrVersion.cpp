#include "QrVersion.hpp"

#include <array>

namespace qrcode {
namespace {

constexpr std::array<int, 0> version1Centers{};
constexpr std::array version2Centers{6, 18};
constexpr std::array version3Centers{6, 22};
constexpr std::array version4Centers{6, 26};

}  // namespace

std::span<const int> QrVersion::alignmentPatternCenters() const {
    switch (number_) {
        case 1:
            return version1Centers;
        case 2:
            return version2Centers;
        case 3:
            return version3Centers;
        case 4:
            return version4Centers;
        default:
            return {};
    }
}

}  // namespace qrcode
