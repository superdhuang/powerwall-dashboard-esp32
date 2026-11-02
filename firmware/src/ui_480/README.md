# Native 480×480 UI

Waveshare ESP32-S3-Touch-LCD-4B 韌體與 `firmware_sim_native480` 共用 480×480 LVGL 介面來源。位置、尺寸、圖表、觸控與字型均以面板像素為準。英文 simulator 使用獨立 target，不會覆蓋繁中程式。

```sh
make firmware_sim_native480 firmware_sim_native480_en
./firmware_sim_native480 --demo --page trend --capture /tmp/trend-zh.ppm --smoke-test
./firmware_sim_native480_en --demo --page trend --capture /tmp/trend-en.ppm --smoke-test
```

Release build/package:

```sh
python3 firmware/tools/package_s3_release.py --release-id v0.1.1
```

See the root [繁體中文 README](../../../README.md) or [English README](../../../README.en.md) for setup and update instructions.
