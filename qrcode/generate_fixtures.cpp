#include <BitMatrix.h>
#include <CharacterSet.h>
#include <qrcode/QRErrorCorrectionLevel.h>
#include <qrcode/QRWriter.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace {

struct Fixture {
    std::string_view filename;
    std::string_view message;
    int version;
    ZXing::CharacterSet encoding;
    ZXing::QRCode::ErrorCorrectionLevel errorCorrectionLevel;
};

void writePng(const ZXing::BitMatrix& bitmap,
              const std::filesystem::path& path) {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(bitmap.width() * bitmap.height()));

    for (int y = 0; y < bitmap.height(); ++y) {
        for (int x = 0; x < bitmap.width(); ++x) {
            pixels.at(static_cast<std::size_t>(y * bitmap.width() + x)) =
                bitmap.get(x, y) ? 0 : 255;
        }
    }

    constexpr int grayscaleChannels = 1;
    const int written =
        stbi_write_png(path.string().c_str(), bitmap.width(), bitmap.height(),
                       grayscaleChannels, pixels.data(), bitmap.width());
    if (written == 0) {
        throw std::runtime_error("Could not write fixture: " + path.string());
    }
}

void generateFixture(const Fixture& fixture,
                     const std::filesystem::path& outputDirectory) {
    constexpr int quietZone = 4;
    const int symbolSize = 17 + 4 * fixture.version;
    const int imageSize = symbolSize + 2 * quietZone;

    ZXing::QRCode::Writer writer;
    writer.setVersion(fixture.version)
        .setMargin(quietZone)
        .setErrorCorrectionLevel(fixture.errorCorrectionLevel)
        .setEncoding(fixture.encoding);

    const auto bitmap =
        writer.encode(std::string{fixture.message}, imageSize, imageSize);
    const auto outputPath = outputDirectory / fixture.filename;
    writePng(bitmap, outputPath);
    std::cout << fixture.filename << " -> " << fixture.message << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: qrcode_generate_fixtures <output-directory>\n";
        return 1;
    }

    const std::filesystem::path outputDirectory{argv[1]};
    if (!std::filesystem::is_directory(outputDirectory)) {
        std::cerr << "Output directory does not exist: " << outputDirectory
                  << '\n';
        return 1;
    }

    constexpr std::array fixtures{
        Fixture{
            .filename = "qr-v2.png",
            .message = "Hello from Luka",
            .version = 2,
            .encoding = ZXing::CharacterSet::ISO8859_1,
            .errorCorrectionLevel =
                ZXing::QRCode::ErrorCorrectionLevel::Quality,
        },
        Fixture{
            .filename = "qr-v3.png",
            .message = "Almost QR Code Reader",
            .version = 3,
            .encoding = ZXing::CharacterSet::ISO8859_1,
            .errorCorrectionLevel =
                ZXing::QRCode::ErrorCorrectionLevel::Quality,
        },
        Fixture{
            .filename = "qr-v4.png",
            .message = "I like Introduction to C++",
            .version = 4,
            .encoding = ZXing::CharacterSet::ISO8859_1,
            .errorCorrectionLevel =
                ZXing::QRCode::ErrorCorrectionLevel::High,
        },
        Fixture{
            .filename = "qr-kanji.png",
            .message = "漢字は格好いい",
            .version = 1,
            .encoding = ZXing::CharacterSet::Shift_JIS,
            .errorCorrectionLevel =
                ZXing::QRCode::ErrorCorrectionLevel::Low,
        },
    };

    try {
        for (const auto& fixture : fixtures) {
            generateFixture(fixture, outputDirectory);
        }
    } catch (const std::exception& error) {
        std::cerr << "Fixture generation failed: " << error.what() << '\n';
        return 1;
    }
}
