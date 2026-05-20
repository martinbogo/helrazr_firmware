#!/usr/bin/env python3
import asyncio
import sys
import os
import io

try:
    from bleak import BleakScanner, BleakClient
except ImportError:
    print("Please install bleak: pip install bleak")
    sys.exit(1)

# ESP32 Custom OTA implementation (V3)
SERVICE_UUID      = "0000ffff-0000-1000-8000-00805f9b34fb"
CHAR_CONTROL_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"
CHAR_DATA_UUID    = "0000ff02-0000-1000-8000-00805f9b34fb"

# Nordic DFU Service (T114)
NORDIC_DFU_SERVICE = "0000fe59-0000-1000-8000-00805f9b34fb"

async def upload_esp32_ota(client, bin_path):
    file_size = os.path.getsize(bin_path)
    print(f"File size: {file_size} bytes")
    
    # Send START command (0x01 + 4 bytes size little-endian)
    start_cmd = bytearray([0x01]) + file_size.to_bytes(4, byteorder='little')
    await client.write_gatt_char(CHAR_CONTROL_UUID, start_cmd, response=True)
    
    chunk_size = 256
    uploaded = 0
    with open(bin_path, "rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            await client.write_gatt_char(CHAR_DATA_UUID, chunk, response=False)
            uploaded += len(chunk)
            print(f"\rUploaded {uploaded}/{file_size} ({(uploaded*100)/file_size:.1f}%)", end="")
            
            # Tiny sleep to not overwhelm the ESP32 BLE stack
            await asyncio.sleep(0.005)
            
    print("\nSending END command...")
    end_cmd = bytearray([0x02])
    await client.write_gatt_char(CHAR_CONTROL_UUID, end_cmd, response=True)
    print("Done! Device should restart.")

async def main(bin_path):
    print("Scanning for Heltec BLE OTA targets...")
    devices = await BleakScanner.discover(timeout=5.0)
    
    esp32_target = None
    nordic_target = None
    
    for d in devices:
        if d.name and "Helrazr-OTA" in d.name:
            esp32_target = d
        elif d.name and "DfuTarg" in d.name:
            nordic_target = d

    if nordic_target and not esp32_target:
        print("-------------------------------------------------------------------------")
        print("Found a Nordic nRF52 device (T114) in Bootloader Mode!")
        print("T114 utilizes the highly-secure Nordic BLE DFU protocol which requires")
        print("signed cryptographic chunking that is outside the scope of this script.")
        print("")
        print("-> To complete the flash on the T114, please open the 'Adafruit Bluefruit")
        print("   LE Connect' mobile app on your iPhone or Android and upload the")
        print("   .pio/build/t114/firmware.zip package.")
        print("-------------------------------------------------------------------------")
        sys.exit(0)
        
    if not esp32_target:
        print("No compatible Helrazr-OTA ESP32 endpoints found.")
        sys.exit(1)
        
    print(f"Found ESP32 OTA Endpoint: {esp32_target.address}")
    
    async with BleakClient(esp32_target) as client:
        print("Connected.")
        await upload_esp32_ota(client, bin_path)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python ota_ble.py <firmware.bin>")
        sys.exit(1)
    asyncio.run(main(sys.argv[1]))
