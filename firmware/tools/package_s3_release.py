#!/usr/bin/env python3
"""Build and archive immutable Chinese/English native-480 S3 release bundles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone

from flash_s3_assets import validate_bundle


LANGUAGES = {
    "zh": "waveshare-esp32s3-touch-lcd-4b-release",
    "en": "waveshare-esp32s3-touch-lcd-4b-en-release",
}
LAYOUT = {
    "bootloader.bin": (0x000000, 0x8000),
    "partitions.bin": (0x008000, 0x1000),
    "boot_app0.bin": (0x00e000, 0x2000),
    "app.bin": (0x010000, 0x3f0000),
    "assets.bin": (0x410000, 0x800000),
    "history-empty.bin": (0xc10000, 0x3e0000),
}


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, **kwargs)


def git_output(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], check=True,
                          text=True, stdout=subprocess.PIPE).stdout.strip()


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_release_build(pio, firmware, env):
    result = run([str(pio), "run", "-d", str(firmware), "-e", env,
                  "-t", "envdump"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    dump = result.stdout
    release_type = "'BUILD_TYPE': 'release'" in dump
    size_optimization = "'-Os'" in dump
    release_define = (
        "-DPOWERWALL_NATIVE480_RELEASE=1" in dump or
        re.search(r"['\"]POWERWALL_NATIVE480_RELEASE['\"]\s*,\s*1", dump) is not None
    )
    if not (release_type and size_optimization and release_define):
        raise RuntimeError(f"{env}: expected release/-Os and POWERWALL_NATIVE480_RELEASE=1")


def package_language(firmware, core, python, spiffsgen, env, language, destination,
                     source_metadata, release_id):
    build = firmware / ".pio/build" / env
    data = destination / ".stage-data"
    empty = destination / ".stage-empty"
    data.mkdir(parents=True)
    empty.mkdir()
    shutil.copy2(firmware / "assets/native480/fonts.pwf", data / "fonts.pwf")

    for name in ("bootloader.bin", "partitions.bin", "boot_app0.bin"):
        source = (core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
                  if name == "boot_app0.bin" else build / name)
        shutil.copy2(source, destination / name)
    shutil.copy2(build / "firmware.bin", destination / "app.bin")
    run([str(python), str(spiffsgen), "0x800000", str(data),
         str(destination / "assets.bin")])
    run([str(python), str(spiffsgen), "0x3e0000", str(empty),
         str(destination / "history-empty.bin")])
    shutil.rmtree(data)
    shutil.rmtree(empty)

    images = []
    for name, (offset, limit) in LAYOUT.items():
        path = destination / name
        images.append({
            "file": name,
            "offset": offset,
            "limit": limit,
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        })
    manifest = {
        "target": "waveshare-esp32s3-touch-lcd-4b",
        "pio_env": env,
        "language": language,
        "flash_bytes": 0x1000000,
        "fonts_bytes": (firmware / "assets/native480/fonts.pwf").stat().st_size,
        "font_count": int.from_bytes(
            (firmware / "assets/native480/fonts.pwf").read_bytes()[16:20], "little"),
        "release_id": release_id,
        "build_type": "release",
        "optimization": "-Os",
        "release_debug_switch": "POWERWALL_NATIVE480_RELEASE=1",
        "source": source_metadata,
        "images": images,
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    validate_bundle(destination)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-id", required=True,
                        help="new immutable directory name under flash_images/.../releases")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.+-]*", args.release_id):
        parser.error("--release-id may contain only letters, numbers, dot, underscore, plus, and hyphen")

    firmware = Path(__file__).resolve().parents[1]
    root = firmware.parent
    core = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    pio = core / "penv/bin/pio"
    python = core / "penv/bin/python"
    spiffsgen = core / "packages/framework-espidf/components/spiffs/spiffsgen.py"
    if not pio.is_file() or not python.is_file() or not spiffsgen.is_file():
        raise SystemExit(f"PlatformIO build/package tools not found under {core}")

    package_root = root / "flash_images/esp32-s3-lcd-4b/releases"
    destination = package_root / args.release_id
    if destination.exists():
        raise SystemExit(f"Refusing to replace existing release directory: {destination}")

    commit = git_output(root, "rev-parse", "HEAD")
    branch = git_output(root, "branch", "--show-current")
    all_status = git_output(root, "status", "--porcelain=v1", "--untracked-files=all")
    firmware_status = subprocess.run(
        ["git", "-C", str(root), "status", "--porcelain=v1", "--untracked-files=all",
         "--", "firmware"], check=True, text=True, stdout=subprocess.PIPE).stdout
    dirty_firmware_paths = []
    for line in firmware_status.splitlines():
        path = line[3:].split(" -> ")[-1]
        if path.startswith("esp32/"):
            path = path[len("esp32/"):]
        if path:
            dirty_firmware_paths.append(path)
    git_tags = git_output(root, "tag", "--points-at", "HEAD").splitlines()
    source_metadata = {
        "git_commit": commit,
        "git_short_commit": commit[:7],
        "git_branch": branch,
        "git_tags": git_tags,
        "repository_dirty": bool(all_status),
        "firmware_source_dirty": bool(dirty_firmware_paths),
        "firmware_dirty_paths": sorted(set(dirty_firmware_paths)),
        "captured_at_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    }

    envs = list(LANGUAGES.values())
    for env in envs:
        validate_release_build(pio, firmware, env)
    run([str(pio), "run", "-d", str(firmware), *sum((["-e", env] for env in envs), [])])

    package_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".s3-release-", dir=package_root) as temporary:
        staged_release = Path(temporary) / args.release_id
        staged_release.mkdir()
        for language, env in LANGUAGES.items():
            bundle = staged_release / language
            bundle.mkdir()
            manifest = package_language(firmware, core, python, spiffsgen, env,
                                        language, bundle, source_metadata, args.release_id)
            # Confirm the archived app is byte-for-byte the PlatformIO app from this build.
            built_app = firmware / ".pio/build" / env / "firmware.bin"
            if sha256(bundle / "app.bin") != sha256(built_app):
                raise RuntimeError(f"{language}: packaged app differs from PlatformIO output")
            print(f"{language}: app SHA-256 {manifest['images'][3]['sha256']}", flush=True)

        release_tag = args.release_id if args.release_id in git_tags else "unverified"
        firmware_state = "clean" if not dirty_firmware_paths else "dirty"
        workspace_note = ("The firmware source tree was clean; unrelated workspace changes were not part of this build."
                          if not dirty_firmware_paths else
                          "Firmware source changes at build time are listed in each language manifest.")
        workspace_note_zh = ("韌體原始碼乾淨；其他工作區變更未納入建置。"
                             if not dirty_firmware_paths else
                             "建置時韌體原始碼變更列於各語言 manifest。")
        readme = f"""# Powerwall S3 480 release {args.release_id}

This package contains Traditional Chinese (`zh/`) and English (`en/`) firmware for the Waveshare ESP32-S3-Touch-LCD-4B. See the [Waveshare product page](https://www.waveshare.com/esp32-s3-touch-lcd-4b.htm).

## Use the display

- The upper button on the right side advances through the five dashboard pages, one page per press.
- The lower button opens **Settings**. Use the on-screen tabs to switch between Controls and Info.
- Controls lets you adjust brightness and volume, enable or disable voice announcements, and play the voice test. Settings are saved on the device.
- `zh/` uses Traditional Chinese UI and voice prompts. `en/` uses English UI and voice prompts.
- Release firmware removes developer serial preview commands, the manual-history-save key, boot timing, Gateway/heap summaries, and PCM timing traces. Normal startup, settings, history writes, and automatic voice announcements remain enabled.

## First-time setup

1. On first boot, connect a phone to the `powerwall-dashboard` Wi-Fi network and follow the captive portal prompt. If it does not open automatically, browse to `http://192.168.4.1/`.
2. On a new device the Gateway IP and password fields start blank. Enter your home Wi-Fi name and password, the Powerwall Gateway IP and password, and your time zone. Existing Gateway values saved in NVS are loaded before the blank defaults. The time-zone default is UTC+08:00; whole-hour offsets from UTC−12:00 through UTC+14:00 are supported and stored in NVS.
3. After the device joins Wi-Fi, press the lower right button to open Settings, then select **Info** and read **Settings URL**. From a phone or computer on the same Wi-Fi, open that displayed address to revisit the settings page; it uses the device IP and root path `/`.

## Install while preserving settings and history

Select a language, verify the hashes, then run a dry check:

```sh
cd /path/to/esp32
(cd flash_images/esp32-s3-lcd-4b/releases/{args.release_id} && shasum -a 256 -c SHA256SUMS)
./write-rel.sh --dry-run       # Traditional Chinese
./write-rel-en.sh --dry-run    # English
```

After checking the dry run and the board port, rerun the selected command without `--dry-run`. The release wrappers are pinned to the `zh/` and `en/` bundles for this version. `--update-only` writes only the app and font assets; it preserves NVS settings and recorded history. The root `write.sh` and `write-en.sh` scripts currently point at these same bundles. Do not use `--full` for a normal update. `history-empty.bin` is included only for explicit full initialization.

The images under `flash_images/` are local ignored artifacts and are not stored in the source commit or tag. Recreate them from tagged source with `python3 firmware/tools/package_s3_release.py --release-id {args.release_id}`.

## Build source

- Release tag: `{release_tag}`; source commit: `{commit}` on `{branch}`.
- Firmware source tree at build time: `{firmware_state}`. {workspace_note}
- Build type: PlatformIO `release` (`-Os`), with `POWERWALL_NATIVE480_RELEASE=1`.
- Each language `manifest.json` records its build target and image hashes. `SHA256SUMS` covers this README, both manifests, and every image.

---

# Powerwall S3 480 發布版 {args.release_id}

本套件包含繁體中文 (`zh/`) 與英文 (`en/`) 韌體，供 Waveshare ESP32-S3-Touch-LCD-4B 使用。開發板購買頁請見 [Waveshare 官方產品頁](https://www.waveshare.com/esp32-s3-touch-lcd-4b.htm)。

## 螢幕操作

- 右側上鍵每按一次切換至下一個儀表板頁面，五頁依序循環。
- 右側下鍵開啟「設定」，使用畫面分頁切換「控制」與「資訊」。
- 「控制」可調整亮度、音量、語音播報開關，並播放語音測試；設定會保存在裝置上。
- `zh/` 使用繁體中文介面與語音；`en/` 使用英文介面與語音。
- 發布版移除開發用序列埠預覽指令、手動寫入歷史資料鍵、開機計時、Gateway／heap 摘要及 PCM 計時輸出；正常開機、設定、歷史寫入與自動語音播報仍保留。

## 首次設定

1. 首次開機時，手機連線至 Wi-Fi `powerwall-dashboard`，並依提示開啟 captive portal。若未自動跳出，請瀏覽 `http://192.168.4.1/`。
2. 新裝置的 Gateway IP 與密碼欄位預設空白。輸入家中 Wi-Fi 名稱與密碼、Powerwall Gateway IP 與密碼，並選擇時區；韌體會優先載入已存於 NVS 的 Gateway 值。時區預設 UTC+08:00，可選 UTC−12:00 至 UTC+14:00，每小時一格，並保存在裝置 NVS。
3. 裝置連上 Wi-Fi 後，按右側下鍵開啟「設定」，切至「資訊」並查看「設定頁網址」。同一個 Wi-Fi 網路上的手機或電腦可使用該網址重新開啟設定頁；網址使用裝置 IP 與根路徑 `/`。

## 保留裝置設定與歷史資料的更新方式

選擇語言並驗證雜湊後，先執行 dry-run：

```sh
cd /path/to/esp32
(cd flash_images/esp32-s3-lcd-4b/releases/{args.release_id} && shasum -a 256 -c SHA256SUMS)
./write-rel.sh --dry-run       # 繁體中文
./write-rel-en.sh --dry-run    # English
```

確認 dry-run 和序列埠後，再移除 `--dry-run` 執行。release wrappers 固定使用本版本的 `zh/` 與 `en/` 套件；`--update-only` 只更新 app 與字型 assets，保留 NVS 設定及已記錄歷史。根目錄 `write.sh` 和 `write-en.sh` 目前也指向相同套件。一般更新不要使用 `--full`；套件內的 `history-empty.bin` 僅供明確的完整初始化使用。

`flash_images/` 位於 Git 忽略清單中，映像是本機交付物，不包含於 source commit 或 tag。可從標籤原始碼執行 `python3 firmware/tools/package_s3_release.py --release-id {args.release_id}` 重新產生。

## 建置來源

- Release tag：`{release_tag}`；source commit：`{commit}`（branch `{branch}`）。
- 建置時韌體原始碼狀態：`{firmware_state}`。{workspace_note_zh}
- PlatformIO `release` (`-Os`) 建置，啟用 `POWERWALL_NATIVE480_RELEASE=1`。
- 各語言 `manifest.json` 記錄建置環境及每個映像的 SHA-256；`SHA256SUMS` 涵蓋本 README、兩份 manifest 與所有映像。
"""
        (staged_release / "README.md").write_text(readme)
        checksum_lines = []
        for path in sorted(p for p in staged_release.rglob("*") if p.is_file()):
            checksum_lines.append(f"{sha256(path)}  {path.relative_to(staged_release).as_posix()}")
        (staged_release / "SHA256SUMS").write_text("\n".join(checksum_lines) + "\n")
        os.replace(staged_release, destination)

    print(f"Created immutable S3 release: {destination}")


if __name__ == "__main__":
    main()
