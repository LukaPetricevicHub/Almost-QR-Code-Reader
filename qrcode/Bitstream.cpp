#include "Bitstream.hpp"

namespace qrcode {

Bitstream::Bitstream(const std::string& bits) : bits_(bits) {}

bool Bitstream::canRead(int count) const {
    return count >= 0 && position_ + count <= static_cast<int>(bits_.size());
}

std::optional<int> Bitstream::readInt(int count) {
    if (!canRead(count)) {
        return std::nullopt;
    }

    int value = 0;
    for (int i = 0; i < count; ++i) {
        value = value * 2 + (bits_[position_] - '0');
        ++position_;
    }
    return value;
}

}  // namespace qrcode
