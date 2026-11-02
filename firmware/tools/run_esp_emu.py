#!/usr/bin/env python3
"""Run Powerwall firmware in esp-emu, including P4's C6 Wi-Fi coprocessor."""

import argparse
from contextlib import contextmanager
import hashlib
import os
from pathlib import Path
import shutil
import select
import signal
import struct
import subprocess
import sys
import tarfile
import termios
import tempfile
import time
import threading
import urllib.request


ROOT = Path(__file__).resolve().parents[2]
ROM_URL = "https://github.com/espressif/esp-rom-elfs/releases/download/20241011/esp-rom-elfs-20241011.tar.gz"
ROM_SHA256 = "921f000164a421c7628fbfee55b173384aafaa51883adc65cd27bf9b0af9e9a9"


def run(*args):
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True)


def prepare_rom(cache):
    """The existing esp32-p4-evboard target is ES silicon, not the builtin rev3 ROM.

    See espressif/esp-emulator README, 'P4 images built for rev 0.x / 1.x'.
    The archive checksum comes from ESP-IDF v5.5.4 tools/tools.json.
    """
    rom = cache / "esp32p4_rev0_rom.elf"
    archive = cache / "esp-rom-elfs-20241011.tar.gz"
    if not archive.exists():
        print("Downloading Espressif's matching P4 ROM...", flush=True)
        with urllib.request.urlopen(ROM_URL, timeout=60) as source:
            data = source.read()
        if hashlib.sha256(data).hexdigest() != ROM_SHA256:
            raise RuntimeError("ROM archive SHA-256 mismatch")
        archive.write_bytes(data)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != ROM_SHA256:
        raise RuntimeError(f"ROM archive SHA-256 mismatch: {archive}")
    with tarfile.open(archive) as tar:
        members = [m for m in tar.getmembers() if Path(m.name).name == rom.name and m.isfile()]
        if len(members) != 1:
            raise RuntimeError("Expected exactly one P4 rev0 ROM in the verified archive")
        with tar.extractfile(members[0]) as source:
            rom.write_bytes(source.read())
    return rom


def prepare_efuse(path):
    # Local emulator state only; never accesses a physical chip or USB port.
    # QEMU layout: BLK1 starts at word 6. P4 wafer major at BLK1 bit 68.
    # This matches tools/make-efuse.py --chip esp32p4 --chip-rev 1.0.
    words = [0] * 84
    words[6] = 0xC4000001
    words[7] = 0x240A
    words[8] = 1 << 4
    path.write_bytes(struct.pack("<84I", *words))


def prepare_c6(packages, python, state):
    framework = packages / "framework-arduinoespressif32"
    libraries = packages / "framework-arduinoespressif32-libs"
    apps = list((libraries / "hosted").glob("esp32c6-v*.bin"))
    if len(apps) != 1:
        raise RuntimeError(f"Expected one packaged C6 Hosted firmware, found {apps}")
    esptool = packages / "tool-esptoolpy/esptool.py"
    bootloader = state / "c6-bootloader.bin"
    partitions = state / "c6-partitions.bin"
    merged = state / "c6-flash.bin"
    run(python, framework / "tools/gen_esp32part.py",
        framework / "tools/partitions/default.csv", partitions)
    run(python, esptool, "--chip", "esp32c6", "elf2image",
        "--flash-mode", "dio", "--flash-freq", "80m", "--flash-size", "4MB",
        "-o", bootloader, libraries / "esp32c6/bin/bootloader_dio_80m.elf")
    run(python, esptool, "--chip", "esp32c6", "merge-bin", "--flash-size", "4MB",
        "-o", merged, "0x0", bootloader, "0x8000", partitions,
        "0xe000", framework / "tools/partitions/boot_app0.bin", "0x10000", apps[0])
    return merged


def stop(process):
    if process is None or process.poll() is not None:
        return
    process.send_signal(signal.SIGINT)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


@contextmanager
def single_key_input(destination, source_fd=None):
    """Turn each console key into a complete line for esp-emu's line reader."""
    source_fd = sys.stdin.fileno() if source_fd is None else source_fd
    original = None
    done = threading.Event()
    if os.isatty(source_fd):
        original = termios.tcgetattr(source_fd)
        immediate = termios.tcgetattr(source_fd)
        immediate[3] &= ~(termios.ICANON | termios.ECHO)
        immediate[6][termios.VMIN] = 1
        immediate[6][termios.VTIME] = 0
        # Preserve ISIG so Ctrl+C still stops both emulators.
        termios.tcsetattr(source_fd, termios.TCSANOW, immediate)

    def forward():
        try:
            while not done.is_set():
                if not select.select([source_fd], [], [], 0.1)[0]:
                    continue
                data = os.read(source_fd, 128)
                if not data:
                    return
                for key in data:
                    if key in b"12345smwzhSMWZH":
                        destination.write(bytes([key]) + b"\n")
                        destination.flush()
        except (BrokenPipeError, OSError):
            pass  # Child exited or the terminal closed.

    relay = threading.Thread(target=forward, daemon=True)
    try:
        relay.start()
        yield
    finally:
        done.set()
        relay.join(timeout=1)
        if original is not None:
            termios.tcsetattr(source_fd, termios.TCSANOW, original)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("chip", choices=["p4", "s3"], nargs="?", default="p4")
    parser.add_argument("--no-build", action="store_true", help="Use an existing build")
    parser.add_argument("--port", type=int, default=18080, help="Host TCP port forwarded to firmware HTTP 80")
    parser.add_argument("--timeout", help="Optional esp-emu runtime, e.g. 30s")
    args = parser.parse_args()
    emulator = shutil.which("esp-emu")
    if not emulator:
        parser.error("esp-emu must be installed and on PATH")
    environment = "waveshare-esp32p4-emu" if args.chip == "p4" else "guition-4inch-720x720"
    if not args.no_build:
        run("pio", "run", "-d", "firmware", "-e", environment)
    build = ROOT / "firmware/.pio/build" / environment
    firmware = build / "firmware.factory.bin"
    elf = build / "firmware.elf"
    if not firmware.is_file() or not elf.is_file():
        parser.error(f"Missing build output in {build}; run without --no-build")
    cache = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache")) / "powerwall-esp-emu"
    cache.mkdir(parents=True, exist_ok=True)
    packages = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")) / "packages"
    pio_python = packages.parent / "penv/bin/python"
    slave = host = None
    log_path = cache / "c6.log"
    # Unix socket paths are short even on macOS, whose default TMPDIR is long.
    with tempfile.TemporaryDirectory(prefix="pw-emu-", dir="/tmp") as temporary, log_path.open("w") as slave_log:
        state = Path(temporary)
        try:
            network = f"user,hostfwd=tcp::{args.port}-:80"
            command = [emulator, "--chip", "esp32" + args.chip,
                       "--firmware", str(firmware), "--elf", str(elf),
                       "--psram-size", "32M" if args.chip == "p4" else "8M"]
            if args.timeout:
                command += ["--timeout", args.timeout]
            if args.chip == "p4":
                rom = prepare_rom(cache)
                efuse = state / "p4-rev1.efuse"
                prepare_efuse(efuse)
                c6 = prepare_c6(packages, pio_python, state)
                socket = state / "hosted.sock"
                slave = subprocess.Popen(
                    [emulator, "--chip", "esp32c6", "--firmware", str(c6),
                     "--hosted", f"bridge:slave:{socket}", "--net", network],
                    stdin=subprocess.DEVNULL, stdout=slave_log, stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 10
                while not socket.exists():
                    if slave.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError(f"C6 bridge failed to start; see {log_path}")
                    time.sleep(0.05)
                command += ["--rom", str(rom), "--efuse", str(efuse),
                            "--hosted", f"bridge:host:{socket}",
                            "--hosted-reset-gpio", "54", "--hosted-reset-active-high"]
                print(f"P4 ES rev1 + C6 Hosted | C6 log: {log_path}", flush=True)
            else:
                command += ["--net", network]
            host = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.PIPE)
            with single_key_input(host.stdin):
                print("Console: 1-5 / s / m / w / z / h apply immediately; Ctrl+C stops emulation.", flush=True)
                while host.poll() is None:
                    if slave is not None and slave.poll() is not None:
                        raise RuntimeError(f"C6 stopped; see {log_path}")
                    time.sleep(0.1)
            return host.returncode
        finally:
            stop(host)
            stop(slave)
            if host is not None and host.stdin is not None:
                host.stdin.close()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"esp-emu launcher: {error}", file=sys.stderr)
        sys.exit(1)
