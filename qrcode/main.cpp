#include <BitMatrix.h>
#include <HybridBinarizer.h>
#include <ZXingCpp.h>

#include <array>
#include <iostream>
#include <memory>
#include <print>
#include <ranges>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace ZXing;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: qrcode <image>\n";
        return 1;
    }
    std::string filePath = argv[1];
    int width, height, channels;

    std::unique_ptr<stbi_uc, void (*)(void*)> buffer(
        stbi_load(filePath.c_str(), &width, &height, &channels, 0),
        stbi_image_free);
    auto ImageFormatFromChannels =
        std::array{ImageFormat::None, ImageFormat::Lum, ImageFormat::LumA,
                   ImageFormat::RGB, ImageFormat::RGBA};
    ImageView image{buffer.get(), width, height,
                    ImageFormatFromChannels.at(channels)};

    // Crop size is always 4 in the test cases.
    // Change the version if you are not reading Version 1.
    int version = 1;
    auto size = 17 + 4 * version;
    auto cropped = image.cropped(4, 4, size, size);

    // bitmap is a matrix of bool (true: black, false: white).
    auto bitmap = std::make_unique<HybridBinarizer>(cropped)->getBlackMatrix();

    // Draw bitmap to show that it is exactly the QR Code we read in.
    // You can delete this code.
    std::println("bitmap:");
    for (auto i : std::views::iota(0, 21)) {
        for (auto j : std::views::iota(0, 21)) {
            bitmap->get(j, i) == true ? std::print("■ ") : std::print("  ");
        }
        std::println("");
    }

    // You decoder goes here.
    auto isFunctionModule = [size, version](int x, int y) {
        if (x <= 8 && y <= 8) {
            return true;
        }
        if(x >= size - 8 && y <= 8) {
            return true;
        }
        if (x <= 8 && y >= size - 8) {
            return true;
        }
        if (x == 6 || y == 6) {
            return true;
        }
        if (x == 8 && y == 4 * version + 9) {
            return true;
        }
        return false;
    };

    auto maskApplies = [](int mask, int x, int y) {
        auto product = x * y;

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
    };

    auto readMaskFromFormatInformation = [bitmap = bitmap.get()] {
        // QR format bits stored XORed
        constexpr int formatInformationMask = 0b101010000010010;

        auto formatBits = 0;
        auto setFormatBit = [bitmap, &formatBits](int bitIndex, int x, int y) {
            if (bitmap->get(x, y)) {
                formatBits |= 1 << bitIndex;
            }
        };

        for (auto i : std::views::iota(0, 6)) {
            setFormatBit(i, 8, i);
        }
        setFormatBit(6, 8, 7);
        setFormatBit(7, 8, 8);
        setFormatBit(8, 7, 8);

        for (auto i : std::views::iota(9, 15)) {
            setFormatBit(i, 14 - i, 8);
        }

        auto unmaskedFormatBits = formatBits ^ formatInformationMask;
        return (unmaskedFormatBits >> 10) & 0b111;
    };

    auto readBits = [](const std::string& bits, int& position, int count) {
        int value = 0;
        for (int i=0; i < count; ++i) {
            value = value * 2 + (bits.at(position) - '0');
            ++position;
        }
        return value;
    };

    auto appendNumeric = [readBits](const std::string& bits, int& position, std::string& message) {
        int length = readBits(bits, position, 10);
        int groupsOfThree = length / 3;
        int remainingDigits = length % 3;
        int neededBits = groupsOfThree * 10;

        if (remainingDigits == 2) {
            neededBits += 7;
        } else if (remainingDigits == 1) {
            neededBits += 4;
        }

        if (position + neededBits > static_cast<int>(bits.size())) {
            return false;
        }

        for (int group = 0; group < groupsOfThree; ++group) {
            int value = readBits(bits, position, 10);

            if (value > 999) {
                return false;
            }

            message.push_back(static_cast<char>('0' + value / 100));
            message.push_back(static_cast<char>('0' + value / 10%10));
            message.push_back(static_cast<char>('0' + value % 10));
        }

        if (remainingDigits == 2) {
            int value = readBits(bits, position, 7);

            if (value > 99)
                return false;
            message.push_back(static_cast<char>('0' + value / 10));
            message.push_back(static_cast<char>('0' + value % 10));
        } else if (remainingDigits == 1) {
            int value = readBits(bits, position, 4);
            if (value > 9)
                return false;
            message.push_back(static_cast<char>('0' + value));
        }

        return true;
    };

    auto appendAlphanumeric = [readBits](const std::string& bits, int& position, std::string& message) {
        std::string alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

        int length = readBits(bits, position, 9);
        int pairs = length / 2;
        int remainingChars = length % 2;
        int neededBits = pairs * 11 + remainingChars * 6;

        if (position + neededBits > static_cast<int>(bits.size())) {
            return false;
        }

        for (int pair = 0; pair < pairs; ++pair) {
            int value = readBits(bits, position, 11);
            if (value >= 45 * 45)
                return false;
            message.push_back(alphabet.at(value / 45));
            message.push_back(alphabet.at(value % 45));
        }

        if (remainingChars == 1) {
            int value = readBits(bits, position, 6);
            if (value >= 45)
                return false;
            message.push_back(alphabet.at(value));
        }
        return true;
    };

    auto appendByte = [readBits](const std::string& bits, int& position, std::string& message) {
        int length = readBits(bits, position, 8);
        int neededBits = length * 8;
        if (position + neededBits > static_cast<int>(bits.size())) {
            return false;
        }

        for (int i = 0; i < length; ++i) {
            int value = readBits(bits, position, 8);
            if (value < 32 || value > 126)
                return false;
            message.push_back(static_cast<char>(value));
        }

        return true;
    };

    auto decodeMessage = [readBits, appendNumeric, appendAlphanumeric, appendByte](const std::string& bits) {
        int position = 0;
        std::string message;

        while (position + 4 <= static_cast<int>(bits.size())) {
            int mode = readBits(bits, position, 4);

            if (mode == 0) {
                return message;
            }

            bool valid = false;
            if (mode == 1) {
                valid = appendNumeric(bits, position, message);
            } else if (mode == 2) {
                valid = appendAlphanumeric(bits, position, message);
            } else if (mode == 4) {
                valid = appendByte(bits, position, message);
            } else {
                return std::string{};
            }

            if (!valid) {
                return std::string{};
            }
        }
        return message;
    };

    auto mask = readMaskFromFormatInformation();
    std::string bits;
    bits.reserve(208);

    bool upward = true;
    for (int right = size - 1; right > 0; right -= 2) {
        if (right==6) {
            --right;
        }

        for (int step = 0; step < size; ++step) {
            int y = upward ? size - 1 - step : step;

            for(int dx = 0; dx < 2; ++dx) {
                int x = right - dx;

                if (isFunctionModule(x, y)) {
                    continue;
                }
                bool bit = bitmap->get(x, y);

                if (maskApplies(mask, x, y)) {
                    bit = !bit;
                }

                bits.push_back(bit ? '1' : '0');
            }
        }
        upward = !upward;
    }

    std::print("mask {} first 16 bits: ", mask);
    for (int i = 0; i < 16; ++i) {
        std::print("{}", bits.at(i));
    }

    auto decoded = decodeMessage(bits);
    if (!decoded.empty()) {
        std::print(" -> decoded: {}", decoded);
    }
    std::println("");

    // You can compare your results using the solution below.
    ReaderOptions options;
    options.tryHarder(false)
        .tryRotate(false)
        .tryInvert(false)
        .tryDownscale(false)
        .maxNumberOfSymbols(1)
        .isPure(true)
        .returnErrors(true);
    auto barcodes = ReadBarcodes(image, options);
    std::println("{}", barcodes[0].text());
}
