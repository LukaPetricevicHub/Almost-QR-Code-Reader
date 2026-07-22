#pragma once

#include <optional>
#include <string>

namespace qrcode {

class Bitstream;

class Segments {
public:
    [[nodiscard]] static std::optional<std::string> decodeMessage(const std::string& bits);

private:
    static bool appendNumeric(Bitstream& stream, std::string& message);
    static bool appendAlphanumeric(Bitstream& stream, std::string& message);
    static bool appendByte(Bitstream& stream, std::string& message);
};

}  // namespace qrcode
