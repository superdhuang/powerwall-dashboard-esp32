#!/usr/bin/env python3
"""S3-only validated app/assets update, or explicit destructive full initialization."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile
from check_app_image import check_image


def validate_bundle(bundle):
    manifest = json.loads((bundle / 'manifest.json').read_text())
    if manifest.get('target') != 'waveshare-esp32s3-touch-lcd-4b':
        raise ValueError('映像目標不是 S3 LCD 4B')
    expected = {'bootloader.bin': (0, 0x8000), 'partitions.bin': (0x8000, 0x1000),
                'boot_app0.bin': (0xe000, 0x2000), 'app.bin': (0x10000, 0x3f0000),
                'assets.bin': (0x410000, 0x800000), 'history-empty.bin': (0xc10000, 0x3e0000)}
    rows = manifest['images']
    if {row['file'] for row in rows} != set(expected) or len(rows) != len(expected):
        raise ValueError('映像清單不完整')
    for row in rows:
        name = row['file']
        off, limit = expected[name]
        data = (bundle / name).read_bytes()
        if row['offset'] != off or row['limit'] != limit or len(data) != row['bytes'] or not data or len(data) > limit:
            raise ValueError(f'{name} 位址／容量不符')
        if hashlib.sha256(data).hexdigest() != row['sha256']:
            raise ValueError(f'{name} SHA-256 不符')
    partitions = {}
    table = (bundle / 'partitions.bin').read_bytes()
    for pos in range(0, len(table)-31, 32):
        magic, kind, sub, off, size, label, flags = struct.unpack_from('<HBBII16sI', table, pos)
        if magic != 0x50aa:
            continue
        partitions[label.rstrip(b'\0').decode()] = (off, size)
    if partitions != {'otadata': (0xe000, 0x2000), 'app0': (0x10000, 0x3f0000),
                      'nvs': (0x400000, 0x10000), 'assets': (0x410000, 0x800000),
                      'history': (0xc10000, 0x3e0000), 'coredump': (0xff0000, 0x10000)}:
        raise ValueError('分割表不是核准的 16MB 配置')
    check_image(bundle / 'app.bin', bundle / 'partitions.bin')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--port', default='/dev/cu.usbmodem3101')
    parser.add_argument('--full', action='store_true', help='清除整顆 Flash，初始化新配置；設定與歷史會清空')
    parser.add_argument('--update-only', action='store_true', help='鎖定保留 NVS／History 的更新，禁止 --full')
    parser.add_argument('--dry-run', action='store_true', help='校驗映像並列印指令，不開啟串口')
    args = parser.parse_args()
    if args.update_only and args.full:
        parser.error('保留 NVS 更新禁止 --full；不會清除或寫入 Flash。')
    bundle = args.bundle.resolve()
    rows = validate_bundle(bundle)
    tool = Path(os.environ.get('ESPTOOL', str(Path.home()/'.platformio/penv/bin/esptool')))
    if not tool.is_file() or not os.access(tool, os.X_OK):
        raise ValueError(f'找不到 esptool：{tool}')
    common = [str(tool), '--chip', 'esp32s3', '--port', args.port, '--baud', '460800']
    def run(parts):
        print(shlex.join(parts), flush=True)
        if not args.dry_run:
            subprocess.run(parts, check=True)
    with tempfile.TemporaryDirectory(prefix='powerwall-partition-check-') as directory:
        if args.full:
            print('FULL：清除整顆 Flash；Wi-Fi、Gateway、裝置設定與歷史會重設。', flush=True)
            run(common + ['--after', 'no-reset', 'erase-flash'])
        else:
            target = Path(directory)/'partitions.bin'
            print('正在讀取並檢查板上分割表；不會寫入 NVS。', flush=True)
            run(common + ['--after', 'hard-reset', 'read-flash', '0x8000', '0x1000', str(target)])
            if not args.dry_run:
                expected = (bundle/'partitions.bin').read_bytes()
                if target.read_bytes()[:len(expected)] != expected:
                    raise ValueError('板上分割表不相容；已停止，不會修改 NVS 或分割表。')
            print('UPDATE：只更新 app 與 assets，保留 NVS／History。', flush=True)
        # Use the same fresh-connect/default-reset sequence as the original
        # write.sh; do not carry bootloader state across separate tool processes.
        command = common + ['--before', 'default-reset', '--after', 'hard-reset', 'write-flash', '--flash-size', '16MB']
        for row in rows:
            if args.full or row['file'] in ('app.bin', 'assets.bin'):
                command.extend([hex(row['offset']), str(bundle/row['file'])])
        run(command)
    print('DRY RUN 完成；未開啟串口。' if args.dry_run else 'S3 更新完成；esptool 已驗證寫入資料。')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError, KeyError) as exc:
        print(f'[S3 Flash] FAIL: {exc}', file=sys.stderr)
        raise SystemExit(1)
