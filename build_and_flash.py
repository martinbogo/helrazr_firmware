#!/usr/bin/env python3
import sys
import subprocess
import argparse
import os

def show_help():
    print(f"Usage: {os.path.basename(sys.argv[0])} [OPTIONS]")
    print("")
    print("Build and flash Helrazr Firmware for Heltec T114, WiFi LoRa 32 V3, or V4.")
    print("")
    print("Options:")
    print("  -b, --board <board>   Target board: 't114', 'v3', or 'v4'.")
    print("  -p, --port <port>     Serial port for flashing (e.g., /dev/tty.usbmodem101 or COM3).")
    print("                        If no port is provided, the script will only build.")
    print("  -h, --help            Show this help message.")
    print("")
    print("Examples:")
    print(f"  {sys.executable} build_and_flash.py -b t114")
    print(f"  {sys.executable} build_and_flash.py -b v3 -p /dev/ttyUSB0")
    print("")

def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-b", "--board", type=str, help="Target board: 't114', 'v3', or 'v4'")
    parser.add_argument("-p", "--port", type=str, help="Target serial port")
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
    elif board == "v3":
        env = "heltec_v3"
    elif board == "v4":
        env = "heltec_v4"
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
        
    port = args.port
    if port:
        print("")
        print("========================================")
        print(f" Flashing to {port}")
        print("========================================")
        if env == "t114":
            import time
            print("Note: Ensure T114 is in bootloader mode (double-tap reset button) before flashing!")
            time.sleep(2)
            
            nrfutil_script = os.path.join(os.path.expanduser("~"), ".platformio", "packages", "tool-adafruit-nrfutil", "adafruit-nrfutil.py")
            zip_pkg = os.path.join(".pio", "build", "t114", "firmware.zip")
            
            res = subprocess.run([sys.executable, nrfutil_script, "dfu", "serial", "-pkg", zip_pkg, "-p", port, "-b", "115200"])
            if res.returncode != 0:
                 print("Flashing failed!")
                 sys.exit(res.returncode)
        else:
            res = subprocess.run(["pio", "run", "-e", env, "-t", "upload", "--upload-port", port])
            if res.returncode != 0:
                 print("Flashing failed!")
                 sys.exit(res.returncode)
        print("=> Flashing complete!")
    else:
        print("")
        print("=> Build complete. No port specified, skipping flash.")

if __name__ == "__main__":
    main()
