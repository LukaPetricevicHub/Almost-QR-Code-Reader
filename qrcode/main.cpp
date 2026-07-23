#include <BitMatrix.h>
#include <HybridBinarizer.h>
#include <ZXingCpp.h>

#include "Decoder.hpp"
#include "MessageFormatter.hpp"
#include "QrVersion.hpp"
#include "Segments.hpp"

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
    const std::string filePath = argv[1];
    int width = 0;
    int height = 0;
    int channels = 0;

    std::unique_ptr<stbi_uc, void (*)(void*)> buffer(
        stbi_load(filePath.c_str(), &width, &height, &channels, 0),
        stbi_image_free);
    if (!buffer) {
        std::cerr << "Could not load image: " << filePath << '\n';
        return 1;
    }

    constexpr auto imageFormatFromChannels =
        std::array{ImageFormat::None, ImageFormat::Lum, ImageFormat::LumA,
                   ImageFormat::RGB, ImageFormat::RGBA};
    if (channels < 1 || channels >= static_cast<int>(imageFormatFromChannels.size())) {
        std::cerr << "Unsupported number of image channels: " << channels << '\n';
        return 1;
    }

    ImageView image{buffer.get(), width, height,
                    imageFormatFromChannels.at(channels)};

    const auto version = qrcode::QrVersion::fromImageSize(width, height);
    if (!version.has_value()) {
        std::cerr << "Expected a square Version 1-4 QR image with a four-module quiet zone; "
                  << "got " << width << 'x' << height << " pixels\n";
        return 1;
    }

    const int size = version->symbolSize();
    auto cropped = image.cropped(qrcode::QrVersion::quietZoneWidth,
                                 qrcode::QrVersion::quietZoneWidth, size, size);

    // bitmap is a matrix of bool (true: black, false: white).
    auto bitmap = std::make_unique<HybridBinarizer>(cropped)->getBlackMatrix();

    // Draw bitmap to show that it is exactly the QR Code we read in.
    // You can delete this code.
    std::println("Version {} bitmap:", version->number());
    for (const auto i : std::views::iota(0, size)) {
        for (const auto j : std::views::iota(0, size)) {
            bitmap->get(j, i) == true ? std::print("■ ") : std::print("  ");
        }
        std::println("");
    }

    // You decoder goes here.
    const auto decoded = qrcode::Decoder{}.decode(*bitmap, *version);
    if (!decoded.has_value()) {
        std::cerr << "Could not decode QR segments: "
                  << qrcode::toString(decoded.error()) << '\n';
        return 1;
    }

    std::print("version {} mask {} first 16 bits: ", decoded->version,
               decoded->mask);
    for (int i = 0; i < 16; ++i) {
        std::print("{}", decoded->bits.at(i));
    }

    const auto message = qrcode::MessageFormatter::format(decoded->segments);
    if (!message.empty()) {
        std::print(" -> decoded: {}", message);
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
    if (barcodes.empty()) {
        std::cerr << "ZXing comparison decoder found no barcode\n";
        return 1;
    }
    std::println("{}", barcodes[0].text());
}
