#!/bin/sh
PORT="/dev/ttyACM0"
BAUD=9600
echo ""
echo ">>> Opening serial monitor at ${BAUD} baud..."
echo "  (press Ctrl+C to exit)"
echo ""
arduino-cli monitor -p "$PORT" --config baudrate=$BAUD
