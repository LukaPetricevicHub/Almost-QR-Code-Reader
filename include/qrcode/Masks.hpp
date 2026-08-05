#pragma once

namespace qrcode {

class Masks {
public:
    [[nodiscard]] static bool applies(int mask, int x, int y);
};

}  // namespace qrcode
