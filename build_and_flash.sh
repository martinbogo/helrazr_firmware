#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

BOARD=""
PORT=""

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Build and flash Helrazr Firmware for Heltec T114 or WiFi LoRa 32 V3."
    echo ""
    echo "Options:"
    echo "  -b, --board <board>   Target board: 't114' or 'v3'."
    echo "  -p, --port <port>     Serial port for flashing (e.g., /dev/tty.usbmodem101)."
    echo "                        If no port is provided, the script will only build."
    echo "  -h, -?, --help        Show this help message."
    echo ""
    echo "Examples:"
    echo "  $0 -b t114                  # Build only for T114"
    echo "  $0 -b v3 -p /dev/ttyUSB0    # Build and flash V3"
    echo ""
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--board)
            BOARD="$2"
            shift 2
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -h|-\?|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

if [[ -z "$BOARD" ]]; then
    echo "Error: Board not specified."
    show_help
    exit 1
fi

ENV=""
if [[ "$BOARD" == "t114" ]]; then
    ENV="t114"
elif [[ "$BOARD" == "v3" ]]; then
    ENV="heltec_v3"
else
    echo "Error: Invalid board '$BOARD'. Must be 't114' or 'v3'."
    exit 1
fi

echo "========================================"
echo " Building Helrazr Firmware"
echo " Target: $ENV"
echo "========================================"
pio run -e "$ENV"

if [[ -n "$PORT" ]]; then
    echo ""
    echo "========================================"
    echo " Flashing to $PORT"
    echo "========================================"
    
    if [[ "$ENV" == "t114" ]]; then
        echo "Note: Ensure T114 is in bootloader mode (double-tap reset button) before flashing!"
        sleep 2
    fi

    pio run -e "$ENV" -t upload --upload-port "$PORT"
    echo "=> Flashing complete!"
else
    echo ""
    echo "=> Build complete. No port specified, skipping flash."
fi
