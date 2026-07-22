#include "Segments.hpp"

#include "Bitstream.hpp"

namespace qrcode {
namespace {

const std::string alphanumericAlphabet =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

}  // namespace

std::optional<std::string> Segments::decodeMessage(const std::string& bits) {
    Bitstream stream{bits};
    std::string message;

    while (stream.canRead(4)) {
        const auto mode = stream.readInt(4);
        if (!mode.has_value()) {
            return std::nullopt;
        }

        if (*mode == 0) {
            return message;
        }

        bool valid = false;
        if (*mode == 1) {
            valid = appendNumeric(stream, message);
        } else if (*mode == 2) {
            valid = appendAlphanumeric(stream, message);
        } else if (*mode == 4) {
            valid = appendByte(stream, message);
        } else {
            return std::nullopt;
        }

        if (!valid) {
            return std::nullopt;
        }
    }

    return message;
}

bool Segments::appendNumeric(Bitstream& stream, std::string& message) {
    const auto length = stream.readInt(10);
    if (!length.has_value()) {
        return false;
    }

    const int groupsOfThree = *length / 3;
    const int remainingDigits = *length % 3;
    auto neededBits = groupsOfThree * 10;

    if (remainingDigits == 2) {
        neededBits += 7;
    } else if (remainingDigits == 1) {
        neededBits += 4;
    }

    if (!stream.canRead(neededBits)) {
        return false;
    }

    for (int group = 0; group < groupsOfThree; ++group) {
        const auto value = stream.readInt(10);
        if (!value.has_value() || *value > 999) {
            return false;
        }

        message.push_back(static_cast<char>('0' + *value / 100));
        message.push_back(static_cast<char>('0' + *value / 10 % 10));
        message.push_back(static_cast<char>('0' + *value % 10));
    }

    if (remainingDigits == 2) {
        const auto value = stream.readInt(7);
        if (!value.has_value() || *value > 99) {
            return false;
        }
        message.push_back(static_cast<char>('0' + *value / 10));
        message.push_back(static_cast<char>('0' + *value % 10));
    } else if (remainingDigits == 1) {
        const auto value = stream.readInt(4);
        if (!value.has_value() || *value > 9) {
            return false;
        }
        message.push_back(static_cast<char>('0' + *value));
    }

    return true;
}

bool Segments::appendAlphanumeric(Bitstream& stream, std::string& message) {
    const auto length = stream.readInt(9);
    if (!length.has_value()) {
        return false;
    }

    const int pairs = *length / 2;
    const int remainingChars = *length % 2;
    const int neededBits = pairs * 11 + remainingChars * 6;

    if (!stream.canRead(neededBits)) {
        return false;
    }

    for (int pair = 0; pair < pairs; ++pair) {
        const auto value = stream.readInt(11);
        if (!value.has_value() || *value >= 45 * 45) {
            return false;
        }
        message.push_back(alphanumericAlphabet.at(*value / 45));
        message.push_back(alphanumericAlphabet.at(*value % 45));
    }

    if (remainingChars == 1) {
        const auto value = stream.readInt(6);
        if (!value.has_value() || *value >= 45) {
            return false;
        }
        message.push_back(alphanumericAlphabet.at(*value));
    }

    return true;
}

bool Segments::appendByte(Bitstream& stream, std::string& message) {
    const auto length = stream.readInt(8);
    if (!length.has_value()) {
        return false;
    }

    const int neededBits = *length * 8;
    if (!stream.canRead(neededBits)) {
        return false;
    }

    for (int i = 0; i < *length; ++i) {
        const auto value = stream.readInt(8);
        if (!value.has_value() || *value < 32 || *value > 126) {
            return false;
        }
        message.push_back(static_cast<char>(*value));
    }

    return true;
}

}  // namespace qrcode
