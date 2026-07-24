#include "Masks.hpp"

namespace qrcode {

bool Masks::applies(int mask, int x, int y) {
    const auto product = x * y;

    switch (mask) {
        case 0:
            return (x + y) % 2 == 0;
        case 1:
            return y % 2 == 0;
        case 2:
            return x % 3 == 0;
        case 3:
            return (x + y) % 3 == 0;
        case 4:
            return (y / 2 + x / 3) % 2 == 0;
        case 5:
            return (product % 2 + product % 3) == 0;
        case 6:
            return (product % 2 + product % 3) % 2 == 0;
        case 7:
            return ((x + y) % 2 + product % 3) % 2 == 0;
        default:
            return false;
    }
}

}  // namespace qrcode
