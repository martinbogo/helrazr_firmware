#!/usr/bin/env python3
import argparse
import subprocess
import sys
import os

def show_help():
    print("Usage: build_and_ota.py [OPTIONS]")
    print("")
    print("Build and OTA flash Helrazr Firmware for Heltec T114, WiFi LoRa 32 V3, or V4.")
    print("")
    print("Options:")
    print("  -b, --board <board>   Target board: 't114', 'v3', or 'v4'.")
    print("  -h, --help            Show this help message.")
    print("")
    print("Examples:")
    print("  python3 build_and_ota.py -b t114")
    print("  python3 build_and_ota.py -b v3")
    print("")

def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-b", "--board", type=str, help="Target board: 't114', 'v3', or 'v4'")
    parser.add_argument("-h", "--help", action="store_true", help="Show help")
    
    args, unknown = parser.parse_known_args()
    
    if args.help:
        show_help()
        sys.exit(0)
        
    if not args.board:
        print("Error: Board not specified.")
        show_help()
        sys.exit(1)
        
    board = args.board.lower()
    if board == "t114":
        env = "t114"
        firmware_file = ".pio/build/t114/firmware.zip"
        ota_script = "nrf_prn.py"
    elif board == "v3":
        env = "heltec_v3"
        firmware_file = ".pio/build/heltec_v3/firmware.bin"
        ota_script = "ota_ble.py"
    elif board == "v4":
        env = "heltec_v4"
        firmware_file = ".pio/build/heltec_v4/firmware.bin"
        ota_script = "ota_ble.py"
    else:
        print(f"Error: Invalid board '{board}'. Must be 't114', 'v3', or 'v4'.")
        sys.exit(1)
        
    print("========================================")
    print(" Building Helrazr Firmware")
    print(f" Target: {env}")
    print("========================================")
    
    res = subprocess.run(["pio", "run", "-e", env])
    if res.returncode != 0:
        print("Build failed!")
        sys.exit(res.returncode)
        
    if not os.path.exists(firmware_file):
        print(f"Error: Firmware file {firmware_file} not found after build.")
        sys.exit(1)
        
    print("")
    print("========================================")
    print(f" OTA Flashing {firmware_file}")
    print("========================================")
    
    # Run the respective OTA script
    res = subprocess.run([sys.executable, ota_script, firmware_file])
    if res.returncode != 0:
        print("OTA failed!")
        sys.exit(res.returncode)
        
    print("=> OTA Flashing complete!")

if __name__ == "__main__":
    main()
