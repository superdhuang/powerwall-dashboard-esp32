#!/usr/bin/env python3
"""Generate firmware/assets/native480/fonts.pwf from native font sources.

Font names, physical raster sizes, glyph lists, metrics, and fallback names are
declared in assets/native480/font_manifest.json. Glyphs without an outline
source are retained in the small native-size font_fallbacks.pwf asset.
"""
import argparse
import json
import struct
from pathlib import Path

import freetype
from fontTools.ttLib import TTFont

from font_pack_common import parse_pack, rasterize

ROOT = Path(__file__).resolve().parents[1]
LVGL_FONTS = ROOT / ".pio/libdeps/waveshare-esp32s3-touch-lcd-4b/lvgl/scripts/built_in_font"
DEFAULT_SOURCES = {
    "stheiti": "/System/Library/Fonts/STHeiti Medium.ttc",
    "noto": "/tmp/NotoSansCJKtc-Regular.otf",
    "universal": str(ROOT.parent.parent / "tesla_extracted/energy_assets/Energy/Labels/Fonts/UniversalSansText-Medium-540.ttf"),
    "montserrat": str(LVGL_FONTS / "Montserrat-Medium.ttf"),
}
FONT_AWESOME = str(LVGL_FONTS / "FontAwesome5-Solid+Brands+Regular.woff")


def codepoint(value):
    return int(value[2:], 16) if isinstance(value, str) and value.startswith("U+") else int(value)


def gpos_pair_kerning(path, size, cps):
    """ASCII pair kerning in px from GPOS PairPos ('kern' feature) or kern."""
    font = TTFont(path)
    cmap = font.getBestCmap()
    upem = font["head"].unitsPerEm
    gname = {cp: cmap[cp] for cp in cps if cp in cmap}
    rev = {}
    for cp, name in gname.items():
        rev.setdefault(name, []).append(cp)
    pairs = {}

    def add(left, right, value):
        if value and left in rev and right in rev:
            for a in rev[left]:
                for b in rev[right]:
                    pairs.setdefault((a, b), value)

    if "GPOS" in font:
        gpos = font["GPOS"].table
        lookups = set()
        for record in gpos.FeatureList.FeatureRecord:
            if record.FeatureTag == "kern":
                lookups.update(record.Feature.LookupListIndex)
        for index in sorted(lookups):
            lookup = gpos.LookupList.Lookup[index]
            subtables = lookup.SubTable
            if lookup.LookupType == 9:
                subtables = [table.ExtSubTable for table in subtables]
            for table in subtables:
                if getattr(table, "LookupType", 2) != 2 and lookup.LookupType not in (2, 9):
                    continue
                coverage = table.Coverage.glyphs
                if table.Format == 1:
                    for i, first in enumerate(coverage):
                        for pair in table.PairSet[i].PairValueRecord:
                            value = getattr(pair.Value1, "XAdvance", 0) if pair.Value1 else 0
                            add(first, pair.SecondGlyph, value)
                elif table.Format == 2:
                    class1 = table.ClassDef1.classDefs
                    class2 = table.ClassDef2.classDefs
                    for first in coverage:
                        first_class = class1.get(first, 0)
                        for second in gname.values():
                            second_class = class2.get(second, 0)
                            record = table.Class1Record[first_class].Class2Record[second_class]
                            value = getattr(record.Value1, "XAdvance", 0) if record.Value1 else 0
                            add(first, second, value)
    if not pairs and "kern" in font:
        for table in font["kern"].kernTables:
            for (left, right), value in table.kernTable.items():
                add(left, right, value)

    output = {}
    for (left, right), value in pairs.items():
        px = int(round(value * size / upem))
        if px:
            output[(left << 16) | right] = px
    return output


def serialize_pack(records, output_path):
    output = bytearray(24 + 68 * len(records))
    packed = []
    for record in records:
        glyph_records = []
        for cp in sorted(record["glyphs"]):
            glyph = record["glyphs"][cp]
            bitmap_offset = len(output)
            output.extend(glyph["bitmap"])
            glyph_records.append((cp, bitmap_offset, len(glyph["bitmap"]), glyph["adv"],
                                  glyph["w"], glyph["h"], glyph["x"], glyph["y"]))
        glyph_offset = len(output)
        for glyph in glyph_records:
            output.extend(struct.pack("<IIIHHHhhBB", *glyph, 4, 0))
        kerning_offset = len(output)
        for pair in sorted(record["kern"]):
            output.extend(struct.pack("<IhH", pair, record["kern"][pair], 0))
        packed.append((glyph_offset, len(glyph_records), kerning_offset, len(record["kern"])))

    for index, (record, offsets) in enumerate(zip(records, packed)):
        glyph_offset, glyph_count, kerning_offset, kerning_count = offsets
        struct.pack_into("<40sHhhHiIIII", output, 24 + 68 * index,
                         record["name"].encode(), record["height"], record["baseline"],
                         record["underline"], record["thickness"], record["fallback"],
                         glyph_offset, glyph_count, kerning_offset, kerning_count)
    checksum = 2166136261
    for value in output[24:]:
        checksum = ((checksum ^ value) * 16777619) & 0xFFFFFFFF
    struct.pack_into("<8sIIII", output, 0, b"PWFONT1\0", 1, len(output), len(records), checksum)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(output)
    return len(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=ROOT / "assets/native480/font_manifest.json")
    parser.add_argument("--out", type=Path, default=ROOT / "assets/native480/fonts.pwf")
    parser.add_argument("--fallbacks", type=Path)
    parser.add_argument("--stheiti", default=DEFAULT_SOURCES["stheiti"])
    parser.add_argument("--noto", default=DEFAULT_SOURCES["noto"])
    parser.add_argument("--universal", default=DEFAULT_SOURCES["universal"])
    parser.add_argument("--montserrat", default=DEFAULT_SOURCES["montserrat"])
    parser.add_argument("--fontawesome", default=FONT_AWESOME)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text())
    fallback_path = args.fallbacks or (args.manifest.parent / manifest["fallback_asset"])
    fallback_records = {record["name"]: record["glyphs"] for record in parse_pack(fallback_path)}
    sources = {
        "stheiti": args.stheiti,
        "noto": args.noto,
        "universal": args.universal,
        "montserrat": args.montserrat,
    }
    face_awesome = freetype.Face(args.fontawesome)

    font_specs = manifest["fonts"]
    names = [spec["name"] for spec in font_specs]
    if len(names) != len(set(names)):
        raise ValueError("font manifest contains duplicate record names")
    indices = {name: index for index, name in enumerate(names)}
    records = []
    for spec in font_specs:
        source_key = spec["source"]
        if source_key not in sources:
            raise ValueError(f"unknown font source {source_key!r} for {spec['name']}")
        size = int(spec["pixel_size"])
        if size <= 0:
            raise ValueError(f"invalid raster size for {spec['name']}: {size}")
        face = freetype.Face(sources[source_key], int(spec.get("face_index", 0)))
        face.set_pixel_sizes(0, size)
        face_awesome.set_pixel_sizes(0, size)

        glyphs = {}
        fallback_glyphs = fallback_records.get(spec["name"], {})
        required_fallbacks = {codepoint(cp) for cp in spec.get("fallback_glyphs", [])}
        if not required_fallbacks.issubset(fallback_glyphs):
            missing = sorted(required_fallbacks - fallback_glyphs.keys())
            raise ValueError(f"missing retained glyphs for {spec['name']}: {missing}")
        for value in spec["glyphs"]:
            cp = codepoint(value)
            if cp in fallback_glyphs:
                glyphs[cp] = fallback_glyphs[cp]
            elif face.get_char_index(cp):
                glyphs[cp] = rasterize(face, cp)
            elif source_key == "montserrat" and cp >= 0xF000 and face_awesome.get_char_index(cp):
                glyphs[cp] = rasterize(face_awesome, cp)
            else:
                raise ValueError(f"no outline or retained native glyph for {spec['name']} U+{cp:04X}")
            if cp in (0x20, 0x3000):
                glyphs[cp]["adv"] = max(glyphs[cp]["adv"], 1)

        kern = {}
        if spec["kerning"]:
            ascii_codepoints = [cp for cp in glyphs if 32 <= cp <= 126]
            kern = gpos_pair_kerning(sources[source_key], size, ascii_codepoints)
        fallback_name = spec["fallback"]
        if fallback_name is not None and fallback_name not in indices:
            raise ValueError(f"unknown fallback font {fallback_name!r} for {spec['name']}")
        records.append({
            "name": spec["name"],
            "height": int(spec["height"]),
            "baseline": int(spec["baseline"]),
            "underline": int(spec["underline"]),
            "thickness": int(spec["thickness"]),
            "fallback": indices[fallback_name] if fallback_name is not None else -1,
            "glyphs": glyphs,
            "kern": kern,
        })
        print(f"{spec['name']:32s} {size:>2}px h={spec['height']:>2} "
              f"glyphs={len(glyphs)} kern={len(kern)}")

    total = serialize_pack(records, args.out)
    print(f"Font pack: {total} bytes -> {args.out}")


if __name__ == "__main__":
    main()
