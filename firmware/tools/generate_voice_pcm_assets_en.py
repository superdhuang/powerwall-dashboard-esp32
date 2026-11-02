#!/usr/bin/env python3
"""Generate the English S3 voice PCM source from its 16 kHz WAV clips."""
from pathlib import Path
import struct
import wave


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/voice/en"
OUTPUT = ROOT / "src/voice_pcm_assets_en.cpp"
CLIPS = (
    ("Charging", "charging.wav"),
    ("Discharging", "discharging.wav"),
    ("OffGrid", "offgrid.wav"),
)


def read_clip(path):
    with wave.open(str(path), "rb") as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, 16000):
            raise ValueError(f"{path}: expected mono PCM16 at 16 kHz")
        frame_count = wav.getnframes()
        if frame_count <= 0:
            raise ValueError(f"{path}: empty clip")
        raw = wav.readframes(frame_count)
        if len(raw) != frame_count * 2:
            raise ValueError(f"{path}: truncated PCM data")
    return struct.unpack(f"<{frame_count}h", raw)


def format_array(name, samples):
    lines = [f"const int16_t kVoice{name}Pcm[] = {{"]
    for start in range(0, len(samples), 12):
        values = ", ".join(str(value) for value in samples[start:start + 12])
        lines.append(f"  {values},")
    lines.append("};")
    lines.append(f"const size_t kVoice{name}PcmSamples = sizeof(kVoice{name}Pcm) / sizeof(kVoice{name}Pcm[0]);")
    return "\n".join(lines)


def main():
    blocks = []
    for name, filename in CLIPS:
        blocks.append(format_array(name, read_clip(ASSETS / filename)))
    source = (
        "// Generated from firmware/assets/voice/en/*.wav: mono signed PCM16 at 16 kHz.\n"
        "#include \"voice_pcm_assets.hpp\"\n\n"
        "#if defined(POWERWALL_NATIVE480_LANG_EN)\n"
        + "\n\n".join(blocks)
        + "\n#endif\n"
    )
    OUTPUT.write_text(source)
    print(f"Generated {OUTPUT}")


if __name__ == "__main__":
    main()
