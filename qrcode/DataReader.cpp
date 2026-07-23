#include "DataReader.hpp"

#include "Masks.hpp"

#include <stdexcept>

namespace qrcode {

DataReader::DataReader(const ZXing::BitMatrix& bitmap, QrVersion version, int mask)
    : bitmap_(bitmap), version_(version), mask_(mask), size_(version.symbolSize()) {
    if (bitmap.width() != size_ || bitmap.height() != size_) {
        throw std::invalid_argument("QR bitmap size does not match its version");
    }
    if (mask < 0 || mask > 7) {
        throw std::invalid_argument("QR mask must be between 0 and 7");
    }
}

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
    if (isAlignmentPattern(x, y)) {
        return true;
    }
    if (x == 8 && y == 4 * version_.number() + 9) {
        return true;
    }
    return false;
}

bool DataReader::isAlignmentPattern(int x, int y) const {
    const auto centers = version_.alignmentPatternCenters();
    if (centers.empty()) {
        return false;
    }

    const int firstCenter = centers.front();
    const int lastCenter = centers.back();

    for (const int centerY : centers) {
        for (const int centerX : centers) {
            const bool overlapsFinder =
                (centerX == firstCenter && centerY == firstCenter) ||
                (centerX == firstCenter && centerY == lastCenter) ||
                (centerX == lastCenter && centerY == firstCenter);
            if (overlapsFinder) {
                continue;
            }

            if (x >= centerX - 2 && x <= centerX + 2 &&
                y >= centerY - 2 && y <= centerY + 2) {
                return true;
            }
        }
    }

    return false;
}

}  // namespace qrcode
