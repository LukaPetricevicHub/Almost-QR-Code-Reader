#include "DataReader.hpp"

#include "Masks.hpp"

namespace qrcode {

DataReader::DataReader(const ZXing::BitMatrix& bitmap, int version, int mask)
    : bitmap_(bitmap), version_(version), mask_(mask), size_(17 + 4 * version) {}

std::string DataReader::readBits() const {
    std::string bits;
    bits.reserve(size_ * size_);

    bool upward = true;
    for (int right = size_ - 1; right > 0; right -= 2) {
        if (right == 6) {
            --right;
        }

        for (int step = 0; step < size_; ++step) {
            const int y = upward ? size_ - 1 - step : step;

            for (int dx = 0; dx < 2; ++dx) {
                const int x = right - dx;

                if (isFunctionModule(x, y)) {
                    continue;
                }

                auto bit = bitmap_.get(x, y);
                if (Masks::applies(mask_, x, y)) {
                    bit = !bit;
                }

                bits.push_back(bit ? '1' : '0');
            }
        }
        upward = !upward;
    }

    return bits;
}

bool DataReader::isFunctionModule(int x, int y) const {
    if (x <= 8 && y <= 8) {
        return true;
    }
    if (x >= size_ - 8 && y <= 8) {
        return true;
    }
    if (x <= 8 && y >= size_ - 8) {
        return true;
    }
    if (x == 6 || y == 6) {
        return true;
    }
    if (x == 8 && y == 4 * version_ + 9) {
        return true;
    }
    return false;
}

}  // namespace qrcode
