#!/bin/sh

FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc"
PORT="/dev/ttyACM0"
SKETCH="firmware.ino"
BAUD=9600

echo "========================================"
echo "  AA AEROSPACE ESPTOOL FLASHER"
echo "========================================"
echo "  Board : $FQBN"
echo "  Port  : $PORT"
echo "  Sketch: $SKETCH"
echo "========================================"

echo ""
echo ">>> [1/3] Compiling..."
arduino-cli compile --fqbn "$FQBN" --build-path ./build --export-binaries "$SKETCH"

if [ $? -ne 0 ]; then
  echo ""
  echo "✗ Compile failed. Aborting."
  exit 1
fi
echo "✓ Compile successful."

echo ""
echo ">>> [2/3] Uploading to $PORT..."
arduino-cli upload --verbose -p "$PORT" --fqbn "$FQBN" --build-path ./build "$SKETCH"

if [ $? -ne 0 ]; then
  echo ""
  echo "✗ Upload failed!"
  exit 1
fi
echo "✓ Upload successful."

echo ""
echo ">>> [3/3] Opening serial monitor at ${BAUD} baud..."
./serial.sh
