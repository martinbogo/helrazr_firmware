import asyncio
import struct
import zipfile
import sys
import json
from bleak import BleakClient, BleakScanner

CTRL = "00001531-1212-efde-1523-785feabcd123"
PKT  = "00001532-1212-efde-1523-785feabcd123"

class LegacyDFU:
    def __init__(self, client):
        self.client = client
        self.q = asyncio.Queue()
        self.prn_event = asyncio.Event()

    def cb(self, h, d):
        #print(f"DEBUG NOTIFY: {d.hex()}")
        if len(d) > 0 and d[0] == 0x11:
            self.prn_event.set()
        else:
            self.q.put_nowait(d)

    async def send_cmd(self, payload):
        print(f"DEBUG SEND: {payload.hex()}")
        await self.client.write_gatt_char(CTRL, payload, response=True)

    async def expect(self, op):
        while True:
            try:
                res = await asyncio.wait_for(self.q.get(), 10.0)
                if res[0] == 0x10 and res[1] == op:
                    if res[2] != 0x01:
                        raise Exception(f"DFU failed: {res.hex()} for op {op}")
                    return
            except asyncio.TimeoutError:
                raise Exception(f"DFU timeout waiting for op {op}")

async def wait_for_dfu_device():
    print("Scanning for DFU Target...")
    for _ in range(10):
        devices = await BleakScanner.discover(timeout=2.0)
        for d in devices:
            if d.name and ("OTA" in d.name or "Dfu" in d.name or "HT-" in d.name):
                print(f"Found {d.name} at {d.address}")
                return d.address
            
            uuids = d.metadata.get("uuids", [])
            if "00001530-1212-efde-1523-785feabcd123" in uuids or "1530" in uuids or "00001530" in uuids[0] if uuids else False:
                print(f"Found by UUID: {d.address}")
                return d.address
    raise Exception("Could not find DFU Target!")

async def run(zip_path):
    with zipfile.ZipFile(zip_path, 'r') as z:
        manifest = json.loads(z.read("manifest.json"))
        dat_file = z.read(manifest["manifest"]["application"]["dat_file"])
        bin_file = z.read(manifest["manifest"]["application"]["bin_file"])

    mac = await wait_for_dfu_device()

    async with BleakClient(mac) as c:
        print("Connected.")
        d = LegacyDFU(c)
        await c.start_notify(CTRL, d.cb)
        await asyncio.sleep(0.5)
        
        prn_target = 2
        print(f"Setting PRN to {prn_target}")
        await d.send_cmd(struct.pack("<BH", 0x08, prn_target))

        print("Start DFU App")
        await d.send_cmd(struct.pack("<BB", 0x01, 0x04))
        print("Writing sizes to PKT")
        await c.write_gatt_char(PKT, struct.pack("<III", 0, 0, len(bin_file)), response=False)
        await d.expect(0x01)

        print("Sending Init Packet")
        await d.send_cmd(struct.pack("<BB", 0x02, 0x00)) 
        for i in range(0, len(dat_file), 20):
            await c.write_gatt_char(PKT, dat_file[i:i+20], response=False)
            await asyncio.sleep(0.01)
        await d.send_cmd(struct.pack("<BB", 0x02, 0x01))
        await d.expect(0x02)

        print("Flashing BIN with PRN synchronization...")
        await d.send_cmd(struct.pack("<B", 0x03))
        
        bytes_sent = 0
        packets_sent = 0

        for i in range(0, len(bin_file), 20):
            chunk = bin_file[i:i+20]
            
            if packets_sent % prn_target == 0:
                d.prn_event.clear()
                
            await c.write_gatt_char(PKT, chunk, response=False)
            
            bytes_sent += len(chunk)
            packets_sent += 1
            
            if packets_sent % prn_target == 0:
                try:
                    await asyncio.wait_for(d.prn_event.wait(), timeout=10.0)
                except asyncio.TimeoutError:
                    print(f"\nTimeout waiting for PRN at packet {packets_sent}!")
                    return
                    
                print(f"\rSent {bytes_sent}/{len(bin_file)} bytes", end="", flush=True)

        print(f"\nSent {bytes_sent}/{len(bin_file)} bytes")
        print("Validating...")
        await d.send_cmd(struct.pack("<B", 0x04)) 
        await d.expect(0x04)
        
        print("Activating!")
        await d.send_cmd(struct.pack("<B", 0x05))
        print("Success, device will reboot.")
        
asyncio.run(run(sys.argv[1]))
