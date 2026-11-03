#!/usr/bin/env python3
"""Build current S3 sources and package validated app/font images without flashing.

Default: the native 480 Chinese build (env waveshare-esp32s3-touch-lcd-4b) with
assets/native480/fonts.pwf -> flash_images/esp32-s3-lcd-4b/current and /zh.
--lang en builds the separate English app/package at flash_images/.../en.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from flash_s3_assets import validate_bundle


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lang', choices=('zh', 'en'), default='zh',
                        help='native480 UI language; zh is the write.sh current bundle')
    args = parser.parse_args()
    firmware = Path(__file__).resolve().parents[1]
    core = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home() / '.platformio'))
    pio = core / 'penv/bin/pio'
    python = core / 'penv/bin/python'
    target = 'waveshare-esp32s3-touch-lcd-4b' + ('-en' if args.lang == 'en' else '')
    fonts = firmware / 'assets/native480/fonts.pwf'
    subprocess.run([str(pio), 'run', '-d', str(firmware), '-e', target], check=True)
    build = firmware / '.pio/build' / target
    spiffsgen = core / 'packages/framework-espidf/components/spiffs/spiffsgen.py'
    package_root = firmware.parent / 'flash_images/esp32-s3-lcd-4b'
    destination = package_root / ('en' if args.lang == 'en' else 'current')
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Stage and validate everything before replacing the usable bundle.
    with tempfile.TemporaryDirectory(prefix='.s3-package-', dir=destination.parent) as temp:
        stage = Path(temp)
        images = stage / 'images'
        images.mkdir()
        data = stage / 'data'
        empty = stage / 'empty'
        data.mkdir()
        empty.mkdir()
        shutil.copy2(fonts, data / 'fonts.pwf')
        for name, source in {
            'app.bin': build / 'firmware.bin',
            'bootloader.bin': build / 'bootloader.bin',
            'partitions.bin': build / 'partitions.bin',
            'boot_app0.bin': core / 'packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin',
        }.items():
            shutil.copy2(source, images / name)
        for size, directory, name in (
            (0x800000, data, 'assets.bin'),
            (0x3e0000, empty, 'history-empty.bin'),
        ):
            subprocess.run([str(python), str(spiffsgen), hex(size), str(directory),
                            str(images / name)], check=True)
        layout = {'bootloader.bin': (0, 0x8000), 'partitions.bin': (0x8000, 0x1000),
                  'boot_app0.bin': (0xe000, 0x2000), 'app.bin': (0x10000, 0x3f0000),
                  'assets.bin': (0x410000, 0x800000),
                  'history-empty.bin': (0xc10000, 0x3e0000)}
        rows = []
        for name, (offset, limit) in layout.items():
            content = (images / name).read_bytes()
            rows.append(dict(file=name, offset=offset, limit=limit, bytes=len(content),
                             sha256=hashlib.sha256(content).hexdigest()))
        (images / 'manifest.json').write_text(json.dumps({
            # Board id checked by flash_s3_assets.py; both builds target the same 4B board.
            'target': 'waveshare-esp32s3-touch-lcd-4b', 'pio_env': target,
            'language': args.lang, 'flash_bytes': 0x1000000,
            'fonts_bytes': (data / 'fonts.pwf').stat().st_size,
            'font_count': int.from_bytes(fonts.read_bytes()[16:20], 'little'), 'images': rows,
        }, indent=2) + '\n')
        validate_bundle(images)
        publish_to = [destination]
        if args.lang == 'zh':
            publish_to.insert(0, package_root / 'zh')
        for output in publish_to:
            output.mkdir(exist_ok=True)
            with tempfile.TemporaryDirectory(prefix='.s3-publish-', dir=output.parent) as publish_temp:
                staged = Path(publish_temp)
                for source in images.iterdir():
                    shutil.copy2(source, staged / source.name)
                for source in staged.iterdir():
                    os.replace(source, output / source.name)
    print(f'Validated {args.lang} S3 bundle: {destination}', flush=True)


if __name__ == '__main__':
    main()
