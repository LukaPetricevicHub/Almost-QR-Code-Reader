#pragma once

#include <optional>
#include <string>

namespace qrcode {

class Bitstream {
public:
    explicit Bitstream(const std::string& bits);

    [[nodiscard]] bool canRead(int count) const;
    [[nodiscard]] std::optional<int> readInt(int count);

private:
    const std::string& bits_;
    int position_ = 0;
};

}  // namespace qrcode
