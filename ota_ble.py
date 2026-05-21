#!/usr/bin/env python3
import asyncio
import sys
import os
import struct
import json
import zipfile

try:
    from bleak import BleakScanner, BleakClient
except ImportError:
    print("Please install bleak: pip install bleak")
    sys.exit(1)

# ESP32 Custom OTA implementation (V3)
SERVICE_UUID      = "0000ffff-0000-1000-8000-00805f9b34fb"
CHAR_CONTROL_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"
CHAR_DATA_UUID    = "0000ff02-0000-1000-8000-00805f9b34fb"

# Nordic DFU Service (Legacy / Default Bootloader for older softdevices)
NORDIC_DFU_SERVICE = "00001530-1212-efde-1523-785feabcd123"
NORDIC_CP_CHAR     = "00001531-1212-efde-1523-785feabcd123"
NORDIC_PKT_CHAR    = "00001532-1212-efde-1523-785feabcd123"

# =======================================================================
# ESP32-S3 Flash Protocol
# =======================================================================
async def upload_esp32_ota(client, bin_path):
    assert bin_path.endswith('.bin'), "ESP32 OTA requires a .bin file"
    file_size = os.path.getsize(bin_path)
    print(f"File size: {file_size} bytes")
    
    # Workaround for macOS aggressive characteristic caching returning duplicates
    ctrl_char = None
    data_char = None
    for service in client.services:
        if service.uuid.lower() == SERVICE_UUID.lower():
            for char in service.characteristics:
                if char.uuid.lower() == CHAR_CONTROL_UUID.lower() and not ctrl_char:
                    ctrl_char = char
                if char.uuid.lower() == CHAR_DATA_UUID.lower() and not data_char:
                    data_char = char
                    
    start_cmd = bytearray([0x01]) + file_size.to_bytes(4, byteorder='little')
    await client.write_gatt_char(ctrl_char or CHAR_CONTROL_UUID, start_cmd, response=True)
    
    chunk_size = 256
    uploaded = 0
    with open(bin_path, "rb") as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            await client.write_gatt_char(data_char or CHAR_DATA_UUID, chunk, response=False)
            uploaded += len(chunk)
            print(f"\rUploaded {uploaded}/{file_size} ({(uploaded*100)/file_size:.1f}%)", end="")
            await asyncio.sleep(0.005)
            
    print("\nSending END command...")
    end_cmd = bytearray([0x02])
    await client.write_gatt_char(ctrl_char or CHAR_CONTROL_UUID, end_cmd, response=True)
    print("Done! ESP32 should restart.")

# =======================================================================
# NORDIC Secure DFU Flash Protocol (T114)
# =======================================================================
class NordicDFUClient:
    def __init__(self, client):
        self.client = client
        self.notif_queue = asyncio.Queue()

    def callback(self, handle, data):
        self.notif_queue.put_nowait(data)

    async def wait_response(self, expected_op):
        res = await self.notif_queue.get()
        if res[0] != 0x60 or res[1] != expected_op:
            raise Exception(f"Unexpected DFU response: {res.hex()}")
        if res[2] != 0x01: # 1=Success
            raise Exception(f"DFU Operation {expected_op} failed with code {res[2]}")
        return res

    async def select_object(self, obj_type):
        await self.client.write_gatt_char(NORDIC_CP_CHAR, struct.pack("<BB", 0x06, obj_type), response=True)
        res = await self.wait_response(0x06)
        max_size, offset, crc = struct.unpack("<III", res[3:])
        return max_size, offset, crc

    async def create_object(self, obj_type, size):
        await self.client.write_gatt_char(NORDIC_CP_CHAR, struct.pack("<BBI", 0x01, obj_type, size), response=True)
        await self.wait_response(0x01)

    async def set_prn(self, count=0):
        await self.client.write_gatt_char(NORDIC_CP_CHAR, struct.pack("<BH", 0x02, count), response=True)
        await self.wait_response(0x02)

    async def calculate_checksum(self):
        await self.client.write_gatt_char(NORDIC_CP_CHAR, struct.pack("<B", 0x03), response=True)
        res = await self.wait_response(0x03)
        offset, crc = struct.unpack("<II", res[3:])
        return offset, crc

    async def execute(self):
        await self.client.write_gatt_char(NORDIC_CP_CHAR, struct.pack("<B", 0x04), response=True)
        await self.wait_response(0x04)

async def upload_nordic_dfu(client, zip_path):
    assert zip_path.endswith('.zip'), "Nordic DFU requires a .zip package"
    print(f"Extracting {zip_path}...")
    with zipfile.ZipFile(zip_path, 'r') as z:
        manifest = json.loads(z.read("manifest.json"))
        app_manifest = manifest["manifest"]["application"]
        dat_file = z.read(app_manifest["dat_file"])
        bin_file = z.read(app_manifest["bin_file"])

    dfu = NordicDFUClient(client)
    await client.start_notify(NORDIC_CP_CHAR, dfu.callback)
    
    # 1. Send Command (dat file)
    await dfu.set_prn(0)
    await dfu.select_object(0x01) # Command object
    await dfu.create_object(0x01, len(dat_file))
    
    print("Sending init packet (.dat)...")
    for i in range(0, len(dat_file), 20):
        await client.write_gatt_char(NORDIC_PKT_CHAR, dat_file[i:i+20], response=False)
        
    await dfu.calculate_checksum()
    await dfu.execute()
    print("Init packet accepted.")

    # 2. Send Data (bin file)
    max_size, offset, crc = await dfu.select_object(0x02) # Data object
    chunk_size = max_size
    
    print(f"Sending firmware ({len(bin_file)} bytes)...")
    bytes_sent = 0
    while bytes_sent < len(bin_file):
        chunk = bin_file[bytes_sent:bytes_sent+chunk_size]
        await dfu.create_object(0x02, len(chunk))
        
        # Stream chunk in BLE MTU chunks
        for i in range(0, len(chunk), 20):
            await client.write_gatt_char(NORDIC_PKT_CHAR, chunk[i:i+20], response=False)
            
        await dfu.calculate_checksum()
        await dfu.execute()
        bytes_sent += len(chunk)
        print(f"\rUploaded {bytes_sent}/{len(bin_file)} ({(bytes_sent*100)/len(bin_file):.1f}%)", end="")
        
    print("\nUpdate complete. DfuTarg will restart.")

# =======================================================================
# Main Entry Point
# =======================================================================
async def main(file_path):
    print("Scanning for Heltec BLE OTA devices (Helrazr-OTA or DfuTarg)...")
    
    def match_device(device, adv_data):
        # 1) Check exact names
        name = device.name or adv_data.local_name or ""
        if "Helrazr-OTA" in name or "DfuTarg" in name or "HT-" in name or "-OTA" in name:
            return True
        # 2) Check UUIDs for ESP32 target
        uuids_str = str(adv_data.service_uuids).lower()
        if SERVICE_UUID.lower() in uuids_str or "ffff" in uuids_str:
            return True
        # 3) Check UUIDs for Nordic target
        if NORDIC_DFU_SERVICE in adv_data.service_uuids:
            return True
        if NORDIC_DFU_SERVICE.upper() in adv_data.service_uuids:
            return True
        return False
        
    target_device = await BleakScanner.find_device_by_filter(match_device, timeout=5.0)

    if not target_device:
        print("Error: No compatible devices found in OTA mode.")
        sys.exit(1)

    target_type = 'esp32'
    name = target_device.name or ""
    if "DfuTarg" in name or "HT-" in name:
        target_type = 'nordic'
    # Fallback to identify by metadata
    elif NORDIC_DFU_SERVICE in target_device.metadata.get('uuids', []):
        target_type = 'nordic'
        
    print(f"Found {target_type.upper()} endpoint: {name} ({target_device.address})")
    
    try:
        async with BleakClient(target_device) as client:
            print("Connected.")
            if target_type == 'esp32':
                await upload_esp32_ota(client, file_path)
            elif target_type == 'nordic':
                await upload_nordic_dfu(client, file_path)
    except Exception as e:
        print(f"\nError during upload: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python ota_ble.py <firmware_file>")
        print("  - For V3 (ESP32): point to .bin")
        print("  - For T114 (nRF52): point to .zip")
        sys.exit(1)
    asyncio.run(main(sys.argv[1]))
