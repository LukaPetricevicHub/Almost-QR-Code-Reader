#pragma once

#include <BitMatrix.h>

#include "QrVersion.hpp"

#include <string>

namespace qrcode {

class DataReader {
public:
    DataReader(const ZXing::BitMatrix& bitmap, QrVersion version, int mask);

    [[nodiscard]] std::string readBits() const;

private:
    [[nodiscard]] bool isFunctionModule(int x, int y) const;
    [[nodiscard]] bool isAlignmentPattern(int x, int y) const;

    const ZXing::BitMatrix& bitmap_;
    QrVersion version_;
    int mask_;
    int size_;
};

}  // namespace qrcode
