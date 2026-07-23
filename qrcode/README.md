# Almost QR Code Reader

Almost QR Code Reader is my implementation of a QR Code reader for my project in C++. The regression tests run the compiled program as a black-box executable.

## Scope

The decoder works on Model 2 Version 1-4 QR samples. It loads images with `stb_image`, uses ZXing to turn the image into a black/white bitmap, and then decodes the QR bitmap directly. The input is still expected to use one pixel per module and a four-module quiet zone. The ZXing decoded text is printed at the end as a debugging comparison.

Implemented behavior:

- Infers Versions 1-4 from the input dimensions.
- Reads QR data modules in the standard zig-zag order.
- Skips finder, timing, format, dark, and version-specific alignment modules.
- Reads the mask pattern from the QR format information.
- Decodes numeric mode, alphanumeric mode, and byte mode for printable ASCII.

## Code layout

- `main.cpp`: command-line entry point, image loading, bitmap debug printing, and ZXing comparison output.
- `Decoder.cpp`: coordinates mask reading, data-bit reading, and segment decoding.
- `DataReader.cpp`: walks the QR data modules in the standard zig-zag order.
- `Masks.cpp`: implements the 8 QR mask formulas and reads the mask from format information.
- `QrVersion.cpp`: validates Versions 1-4 and provides their sizes and alignment-pattern positions.
- `Bitstream.cpp`: reads fixed-width integer values from the decoded bit string.
- `Segments.cpp`: decodes numeric, alphanumeric, and printable ASCII byte segments.

## Output

The program intentionally prints development output while the decoder is being built. Current output includes:

- the converted QR code 
- the first 16 data bits for the mask stored in the format information,
- the decoded message on the mask line,
- the ZXing decoded text as a final comparison line.

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

The expected result includes the five supplied Version 1 images and one regression image for each new version:

```text
qr01.png -> 12345
qr02.png -> 314159
qr03.png -> Hello World
qr04.png -> Intro. to C++
qr05.png -> 1 + 2 is 3
qr-v2.png -> VERSION 2 / ALIGNMENT
qr-v3.png -> Version 3 has a larger data area.
qr-v4.png -> VERSION 4 / 1234567890 / GENERALIZED DECODER
```
