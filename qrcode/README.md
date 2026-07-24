# Almost QR Code Reader

Almost QR Code Reader is my implementation of a QR Code reader for my project in Introduction to C++. The regression tests run the compiled program as a black-box executable.

## Scope

The decoder works on Model 2 Version 1-4 samples. It loads images with `stb_image`, uses ZXing to turn the image into a black/white bitmap, and then decodes the QR bitmap itself. The input is expected to use one pixel per module and a four block quiet zone.

General QR image detection and perspective correction are outside the project
scope; this is the one advanced assignment task that is not implemented.

Implemented behavior:

- Infers Versions 1-4 from the input dimensions.
- Reads data modules in the standard zig-zag order.
- Skips finder, timing, format, dark, and version specific alignment modules.
- Reads both copies of the QR format information and corrects up to three damaged format bits.
- Reads the error-correction level and mask pattern from the corrected format information.
- Separates data from parity codewords and deinterleaves multi-block Version 3-4 symbols.
- Corrects damaged Version 1 codewords with QR Reed-Solomon error correction at levels L, M, Q, and H.
- Decodes numeric, alphanumeric, full byte, and Kanji segments.
- Preserves arbitrary byte values and escapes non-printable bytes as `\xNN`.
- Converts QR Kanji values from Shift-JIS to UTF-8 for terminal output.

## Code layout

- `main.cpp`: command-line entry point, image loading, normal output, and optional debug output.
- `Decoder.cpp`: coordinates format reading, data-bit reading, error correction, and segment decoding.
- `FormatInformation.cpp`: reads both format copies and performs BCH nearest-pattern recovery.
- `DataReader.cpp`: walks the QR data modules in the standard zig-zag order.
- `Masks.cpp`: implements the 8 QR mask formulas.
- `QrVersion.cpp`: validates Versions 1-4 and provides their sizes and alignment-pattern positions.
- `Codewords.cpp`: converts between the module bit stream and eight-bit QR codewords.
- `GaloisField256.cpp`: implements finite-field arithmetic for the QR primitive polynomial `0x11D`.
- `ReedSolomon.cpp`: detects and corrects damaged Version 1 codewords.
- `Bitstream.cpp`: reads fixed-width integer values from the decoded bit string.
- `Segments.cpp`: parses typed QR segments and reports detailed decoding errors.
- `TextEncoding.cpp`: maps QR Kanji values to Shift-JIS and converts them to UTF-8.
- `MessageFormatter.cpp`: renders text segments and escapes binary byte segments.

## Output

Normal mode writes only the decoded human-readable message to standard output:

```text
12345
```

Pass `--debug` to additionally print the sampled bitmap, version, error-correction level, mask, correction counts, and first 16 corrected data bits:

```bash
cmake-build-debug/qrcode/qrcode qrcode/qr01.png --debug
```

## Usage

From the repository root:

```bash
cmake -S . -B cmake-build-debug -DPROJECT_TOPIC=QRCODE
cmake --build cmake-build-debug --target qrcode
```

Run one example:

```bash
cmake-build-debug/qrcode/qrcode qrcode/qr01.png
```

## Custom fixtures

The Version 2-4 and Kanji PNGs are test fixtures generated with ZXing's independent QR encoder. Build and run the generator from the repository root:

```bash
cmake -S . -B cmake-build-debug -DPROJECT_TOPIC=QRCODE -DQRCODE_BUILD_FIXTURE_GENERATOR=ON
cmake --build cmake-build-debug --target qrcode_generate_fixtures
cmake-build-debug/qrcode/qrcode_generate_fixtures qrcode
```

The generator forces each requested QR version, writes one pixel per module with a four-module quiet zone, and selects Shift-JIS so that the Japanese fixture uses QR Kanji mode. The Version 2 and 3 fixtures use error-correction level Q, Version 4 uses H, and the Kanji fixture uses L. This exercises one-, two-, and four-block codeword layouts.

## Regression tests

Run the C++ unit tests:

```bash
cmake --build cmake-build-debug --target qrcode_tests
cmake-build-debug/qrcode/qrcode_tests
```

Run all registered tests:

```bash
ctest --test-dir cmake-build-debug --output-on-failure
```

Run the script:

```bash
qrcode/test_qrcode.sh
```

The expected result includes the five already provided Version 1 images and one regression image for each new version:

```text
qr01.png -> 12345
qr02.png -> 314159
qr03.png -> Hello World
qr04.png -> Intro. to C++
qr05.png -> 1 + 2 is 3
qr-v2.png -> Hello from Luka
qr-v3.png -> Almost QR Code Reader
qr-v4.png -> I like Introduction to C++
qr-kanji.png -> 漢字は格好いい
```

The compact C++ suite covers all eight masks, numeric, alphanumeric, byte,
and Kanji segments, format-information recovery, supported QR versions,
malformed input, and a known Version 1 Reed-Solomon correction case. The
shell regression suite checks all nine sample images through the command-line
program.
