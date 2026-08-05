#include "MessageFormatter.hpp"

namespace qrcode {
namespace {

void appendByteSegment(std::string& output, const std::string& bytes) {
    constexpr char hexadecimal[] = "0123456789ABCDEF";

    for (const unsigned char byte : bytes) {
        if (byte >= 32 && byte <= 126) {
            output.push_back(static_cast<char>(byte));
            continue;
        }

        output.append("\\x");
        output.push_back(hexadecimal[byte >> 4]);
        output.push_back(hexadecimal[byte & 0x0F]);
    }
}

}  // namespace

std::string MessageFormatter::format(
    std::span<const DecodedSegment> segments) {
    std::string output;

    for (const auto& segment : segments) {
        if (segment.mode == SegmentMode::byte) {
            appendByteSegment(output, segment.data);
        } else {
            output.append(segment.data);
        }
    }

    return output;
}

}  // namespace qrcode
