#include "TextEncoding.hpp"

#include <iconv.h>

#include <string>

namespace qrcode {
namespace {

class IconvHandle {
public:
    IconvHandle() : handle_(iconv_open("UTF-8", "SHIFT_JIS")) {}

    ~IconvHandle() {
        if (isValid()) {
            iconv_close(handle_);
        }
    }

    IconvHandle(const IconvHandle&) = delete;
    IconvHandle& operator=(const IconvHandle&) = delete;

    [[nodiscard]] bool isValid() const {
        return handle_ != reinterpret_cast<iconv_t>(-1);
    }

    [[nodiscard]] iconv_t get() const {
        return handle_;
    }

private:
    iconv_t handle_;
};

}  // namespace

std::expected<std::string, TextEncodingError> shiftJisToUtf8(
    std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) {
        return std::string{};
    }

    IconvHandle converter;
    if (!converter.isValid()) {
        return std::unexpected(TextEncodingError::converterUnavailable);
    }

    std::string input{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    std::string output(bytes.size() * 3 + 1, '\0');

    char* inputPosition = input.data();
    char* outputPosition = output.data();
    std::size_t inputRemaining = input.size();
    std::size_t outputRemaining = output.size();

    const auto result =
        iconv(converter.get(), &inputPosition, &inputRemaining, &outputPosition,
              &outputRemaining);
    if (result == static_cast<std::size_t>(-1) || inputRemaining != 0) {
        return std::unexpected(TextEncodingError::invalidInput);
    }

    output.resize(output.size() - outputRemaining);
    return output;
}

}  // namespace qrcode
