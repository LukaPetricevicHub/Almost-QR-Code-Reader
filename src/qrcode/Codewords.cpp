#include "Codewords.hpp"

namespace qrcode {

std::expected<std::vector<std::uint8_t>, CodewordError> packCodewords(
    std::string_view bits) {
    constexpr int bitsPerCodeword = 8;
    if (bits.size() % bitsPerCodeword != 0) {
        return std::unexpected(CodewordError::invalidBitCount);
    }

    std::vector<std::uint8_t> codewords;
    codewords.reserve(bits.size() / bitsPerCodeword);

    for (std::size_t offset = 0; offset < bits.size();
         offset += bitsPerCodeword) {
        std::uint8_t value = 0;
        for (int bit = 0; bit < bitsPerCodeword; ++bit) {
            value = static_cast<std::uint8_t>(value << 1);
            const char encodedBit = bits.at(offset + bit);
            if (encodedBit == '1') {
                value = static_cast<std::uint8_t>(value | 1);
            } else if (encodedBit != '0') {
                return std::unexpected(CodewordError::invalidBit);
            }
        }
        codewords.push_back(value);
    }

    return codewords;
}

std::expected<std::vector<std::uint8_t>, CodewordError>
deinterleaveDataCodewords(std::span<const std::uint8_t> codewords,
                          QrBlockLayout layout) {
    if (static_cast<int>(codewords.size()) != layout.totalCodewords()) {
        return std::unexpected(CodewordError::invalidCodewordCount);
    }

    std::vector<std::uint8_t> data;
    data.reserve(layout.totalDataCodewords());

    for (int block = 0; block < layout.blockCount; ++block) {
        for (int position = 0; position < layout.dataCodewordsPerBlock;
             ++position) {
            const auto interleavedIndex = static_cast<std::size_t>(
                position * layout.blockCount + block);
            data.push_back(codewords[interleavedIndex]);
        }
    }
    return data;
}

std::string unpackCodewords(std::span<const std::uint8_t> codewords) {
    constexpr int bitsPerCodeword = 8;
    std::string bits;
    bits.reserve(codewords.size() * bitsPerCodeword);

    for (const auto codeword : codewords) {
        for (int bit = bitsPerCodeword - 1; bit >= 0; --bit) {
            bits.push_back(((codeword >> bit) & 1U) != 0 ? '1' : '0');
        }
    }
    return bits;
}

}  // namespace qrcode
