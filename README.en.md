[繁體中文版](README.md)

# Powerwall Dashboard ESP32

Powerwall Gateway dashboard firmware for the Waveshare ESP32-S3-Touch-LCD-4B. See the [Waveshare product page](https://www.waveshare.com/esp32-s3-touch-lcd-4b.htm) for the board.

The v0.1.1 release provides Traditional Chinese and English firmware, spoken alerts, local settings, and on-device history. The repository also includes an LVGL simulator and firmware build/package tools.

## Screenshots

These pages were rendered by this project's simulator with synthetic demo data. They are not device captures and contain no live Gateway data.

### Home

![Home dashboard](assets/screenshots/en-01-home.png)

### Powerwall

![Powerwall](assets/screenshots/en-02-powerwall.png)

### Home and grid

![Home and grid](assets/screenshots/en-03-home-grid.png)

### Today's trend

![Today's trend](assets/screenshots/en-04-today-trend.png)

### Seven-day history

![Seven-day history](assets/screenshots/en-05-week-history.png)

### Settings: Controls

![Settings controls](assets/screenshots/en-06-settings-controls.png)

### Settings: Info

![Settings info](assets/screenshots/en-07-settings-info.png)

## Buttons and first-time setup

- The upper button on the right advances through the five dashboard pages, one page per press.
- The lower button on the right opens Settings. Use the on-screen tabs to switch between Controls and Info.
- Controls lets you adjust brightness and volume, enable or disable voice announcements, and play a voice test. Settings are stored in device NVS.
- Gateway IP and password fields start blank on a new device. Firmware loads existing saved values from NVS first.
- On first boot, connect a phone to the `powerwall-dashboard` Wi-Fi network and follow the captive portal prompt. If it does not open automatically, browse to `http://192.168.4.1/`.
- Enter your Wi-Fi name and password, the Gateway IP and password, and your time zone. The default is UTC+08:00; whole-hour offsets from UTC−12:00 through UTC+14:00 are supported and stored in NVS.
- After the device joins Wi-Fi, open Settings → Info and read **Settings URL**. From a phone or computer on the same Wi-Fi, open that address to revisit the settings page.

## Install v0.1.1

The release binary ZIP contains Traditional Chinese (`zh/`) and English (`en/`) bundles. Extract it into the source directory, verify the hashes, and run a dry check first:

```sh
(cd flash_images/esp32-s3-lcd-4b/releases/v0.1.1 && shasum -a 256 -c SHA256SUMS)
./write-rel.sh --dry-run
./write-rel-en.sh --dry-run
```

After confirming the correct serial port, rerun the selected script without `--dry-run`. Both scripts update only the app and font assets, preserving NVS settings and recorded history. They do not write the image that clears history. The root `write.sh` and `write-en.sh` scripts also point to this v0.1.1 release.

## Build from source

Install PlatformIO Core and Python 3, then run this from the repository root:

```sh
python3 firmware/tools/package_s3_release.py --release-id v0.1.1
pio run -d firmware -e waveshare-esp32s3-touch-lcd-4b-release
make firmware_sim_native480 firmware_sim_native480_en
./firmware_sim_native480 --demo
./firmware_sim_native480_en --demo
```

The command creates `flash_images/esp32-s3-lcd-4b/releases/v0.1.1/{zh,en}` and records the app/assets hashes and source commit in each manifest. `.gitignore` excludes this generated directory; binary release assets are distributed separately from the source repository.

## Simulator source

This repository includes simulator source code, but no compiled simulator binaries. Building it requires SDL2 (on macOS, install it with `brew install sdl2`) and the PlatformIO dependencies:

```sh
make firmware_sim_native480 firmware_sim_native480_en
./run-zh.sh --demo
./run-en.sh --demo
```

The launchers default to synthetic data. Demo mode makes no Gateway requests and does not read local `nvs_config.json` or history files. For LIVE mode, create a local `nvs_config.json` with your own settings (it is ignored by Git) and run the wrapper with `--live`. The private configuration file is not included in the Git repository.

Copy `nvs_config.example.json` to `nvs_config.json`, then fill in your own Gateway address and password. The JSON shape is:

```json
{
  "gateway_ip": "",
  "gateway_password": "",
  "gateway_email": "customer@example.com",
  "site_title": ""
}
```

Launch LIVE using the wrapper's `--live` flag, or pass both Gateway credentials on the command line. Command-line values override the file in memory and are not saved. Supplying only one credential prints an error and exits without echoing a password:

```sh
./run-en.sh --live
./run-en.sh --gateway-ip YOUR_GATEWAY_IP --gateway-password YOUR_GATEWAY_PASSWORD
```

Replace the example values with your own settings. A command-line password may remain in shell history or the process list; use the local `nvs_config.json` when that matters. The private configuration file is ignored by Git and excluded from release archives.
