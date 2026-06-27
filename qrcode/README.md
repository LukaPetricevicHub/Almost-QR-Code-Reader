# Almost QR Code Reader

Almost QR Code Reader is my implementation of a QR Code reader for my project in C++. The decoder lives in `main.cpp`. The regression tests run the compiled program as a black-box executable.

## Scope

The decoder works Model 2 Version 1 QR samples. It loads images with `stb_image`, uses ZXing to turn it into binary data and make the final comparison result and then decodes the 21x21 QR bitmap directly.

Implemented behavior:

- Reads QR data modules in the standard zig-zag order.
- Skips function modules manually.
- Tries all 8 QR masks.
- Decodes numeric mode, alphanumeric mode, and byte mode for printable ASCII.

## Output

The program intentionally prints development output while the decoder is being built. Current output includes:

- the converted QR code 
- the first 16 data bits for each mask,
- the decoded message on the mask line that produces a valid message.

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
