#pragma once

#include <BitMatrix.h>

#include <string>

namespace qrcode {

class DataReader {
public:
    DataReader(const ZXing::BitMatrix& bitmap, int version, int mask);

    [[nodiscard]] std::string readBits() const;

private:
    [[nodiscard]] bool isFunctionModule(int x, int y) const;

    const ZXing::BitMatrix& bitmap_;
    int version_;
    int mask_;
    int size_;
};

}  // namespace qrcode
