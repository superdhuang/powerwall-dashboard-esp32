#!/usr/bin/env python3
"""Validate the actual boot image against its app partition (CLI and SCons hook)."""
import argparse
from pathlib import Path
import struct


def check_image(image, partition_table):
    image, partition_table = Path(image), Path(partition_table)
    table = partition_table.read_bytes()
    slot = None
    for pos in range(0, len(table) - 31, 32):
        magic, kind, subtype, offset, size = struct.unpack_from('<HBBII', table, pos)
        if magic == 0x50AA and kind == 0 and offset == 0x10000:
            slot = size
            break
    if slot is None:
        raise ValueError('找不到位址 0x10000 的 app 分割區')
    data = image.read_bytes()
    if not data or data[0] != 0xE9:
        raise ValueError('不是有效的 ESP app image')
    if len(data) > slot:
        raise ValueError(f'app image 超限：{len(data)} bytes > {slot} bytes；拒絕交付或燒錄')
    print(f'[AppImage] PASS: {len(data)} / {slot} bytes; remaining {slot-len(data)} bytes')
    return len(data), slot


if 'Import' in globals():
    Import('env')
    def validate_after_build(source, target, env):
        try:
            check_image(env.subst('$BUILD_DIR/${PROGNAME}.bin'), env.subst('$BUILD_DIR/partitions.bin'))
        except (ValueError, OSError) as exc:
            print(f'[AppImage] FAIL: {exc}')
            env.Exit(1)
    env.AddPostAction('$BUILD_DIR/${PROGNAME}.bin', validate_after_build)
elif __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image')
    parser.add_argument('partition_table')
    args = parser.parse_args()
    try:
        check_image(args.image, args.partition_table)
    except (ValueError, OSError) as exc:
        parser.exit(1, f'[AppImage] FAIL: {exc}\n')
