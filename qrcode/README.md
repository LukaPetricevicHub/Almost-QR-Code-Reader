# Almost QR Code Reader

Almost QR Code Reader is my implementation of a QR Code reader for my project in C++. The regression tests run the compiled program as a black-box executable.

## Scope

The decoder works on Model 2 Version 1 QR samples. It loads images with `stb_image`, uses ZXing to turn the image into a black/white bitmap, then decodes the 21x21 QR bitmap directly. The ZXing decoded text is still printed at the end as a debugging comparison.

Implemented behavior:

- Reads QR data modules in the standard zig-zag order.
- Skips function modules manually.
- Reads the mask pattern from the QR format information.
- Decodes numeric mode, alphanumeric mode, and byte mode for printable ASCII.

## Code layout

- `main.cpp`: command-line entry point, image loading, bitmap debug printing, and ZXing comparison output.
- `Decoder.cpp`: coordinates mask reading, data-bit reading, and segment decoding.
- `DataReader.cpp`: walks the QR data modules in the standard zig-zag order.
- `Masks.cpp`: implements the 8 QR mask formulas and reads the mask from format information.
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

Run the script:

```bash
qrcode/test_qrcode.sh
```

The expected result is:

```text
qr01.png -> 12345
qr02.png -> 314159
qr03.png -> Hello World
qr04.png -> Intro. to C++
qr05.png -> 1 + 2 is 3
```
