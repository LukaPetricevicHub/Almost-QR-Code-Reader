#pragma once

#include "Segments.hpp"

#include <span>
#include <string>

namespace qrcode {

class MessageFormatter {
public:
    [[nodiscard]] static std::string format(
        std::span<const DecodedSegment> segments);
};

}  // namespace qrcode
