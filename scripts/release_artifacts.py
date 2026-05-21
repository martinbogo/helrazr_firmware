import os
import shutil
import subprocess

Import("env")

def generate_uf2(source, target, env):
    build_dir = env.get("PROJECT_BUILD_DIR")
    env_name = env.get("PIOENV")
    hex_path = os.path.join(build_dir, env_name, "firmware.hex")
    uf2_path = os.path.join(build_dir, env_name, "firmware.uf2")
    
    print(f"\n[Artifacts] Generating UF2 for T114...")
    # call uf2conv.py from scripts directory
    uf2conv_path = os.path.join("scripts", "uf2conv.py")
    res = subprocess.run([env.get("PYTHONEXE"), uf2conv_path, hex_path, "-c", "-f", "0xADA52840", "-o", uf2_path])
    if res.returncode == 0:
        print(f"[Artifacts] Created: {uf2_path}")

def generate_factory_bin(source, target, env):
    print(f"\n[Artifacts] Generating merged factory.bin for V3...")
    build_dir = env.get("PROJECT_BUILD_DIR")
    env_name = env.get("PIOENV")
    out_dir = os.path.join(build_dir, env_name)
    factory_bin = os.path.join(out_dir, "firmware.factory.bin")
    
    try:
        python_exe = env.get("PYTHONEXE")
        esptool_path = os.path.join(env.get("PROJECT_PACKAGES_DIR"), "tool-esptoolpy", "esptool.py")

        cmd = [
            python_exe, esptool_path,
            "--chip", "esp32s3",
            "merge_bin",
            "-o", factory_bin,
            "--flash_mode", "keep",
            "--flash_freq", "keep",
            "--flash_size", "keep",
            "0x0", os.path.join(out_dir, "bootloader.bin"),
            "0x8000", os.path.join(out_dir, "partitions.bin"),
            "0xe000", os.path.join(env.get("PROJECT_PACKAGES_DIR"), "framework-arduinoespressif32", "tools", "partitions", "boot_app0.bin"),
            "0x10000", os.path.join(out_dir, "firmware.bin")
        ]
        
        res = subprocess.run(cmd)
        if res.returncode == 0:
             print(f"[Artifacts] Created: {factory_bin}")
    except Exception as e:
        print(f"[Artifacts] Error creating factory bin: {e}")

platform = env.get("PIOPLATFORM", "")
if "nrf52" in platform:
    env.AddPostAction("buildprog", generate_uf2)
elif "espressif32" in platform:
    env.AddPostAction("buildprog", generate_factory_bin)

