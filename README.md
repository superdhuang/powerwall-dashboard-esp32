[English version](README.en.md)

# Powerwall Dashboard ESP32

本專案提供 Powerwall Gateway 儀表板韌體，支援 Waveshare ESP32-S3-Touch-LCD-4B。開發板購買頁請見 [Waveshare 官方產品頁](https://www.waveshare.com/esp32-s3-touch-lcd-4b.htm)。

v0.1.1 提供繁體中文與英文介面、語音播報、本機設定與裝置歷史資料；repo 也包含 LVGL simulator 及韌體建置、打包工具。

## 畫面預覽

以下畫面由本專案 simulator 以合成示範資料產生，沒有使用裝置截圖或真實 Gateway 資料。

### 首頁

![首頁](assets/screenshots/zh-01-home.png)

### Powerwall

![Powerwall](assets/screenshots/zh-02-powerwall.png)

### 電網與住家

![電網與住家](assets/screenshots/zh-03-home-grid.png)

### 今日趨勢

![今日趨勢](assets/screenshots/zh-04-today-trend.png)

### 七日歷史

![七日歷史](assets/screenshots/zh-05-week-history.png)

### 設定：控制

![設定控制](assets/screenshots/zh-06-settings-controls.png)

### 設定：資訊

![設定資訊](assets/screenshots/zh-07-settings-info.png)

## 按鍵與首次設定

- 右側上鍵切換五個儀表板頁面；每按一次前進一頁。
- 右側下鍵開啟設定（Settings）；畫面分頁可切換控制（Controls）與資訊（Info）。
- 控制頁可調亮度、音量、語音播報與播放語音測試；設定保存在裝置 NVS。
- 新裝置的 Gateway IP 與密碼欄位預設空白。若裝置已有儲存設定，韌體會先載入 NVS 值。
- 首次開機時，手機連線至 Wi-Fi `powerwall-dashboard`，並依提示開啟 captive portal；若未自動跳出，前往 `http://192.168.4.1/`。
- 輸入家中 Wi-Fi 名稱與密碼、Gateway IP 與密碼，並選擇時區。時區預設 UTC+08:00，可選 UTC−12:00 至 UTC+14:00，每小時一格；設定會保存在 NVS。
- 連線完成後，在設定（Settings）的資訊（Info）頁查看 Settings URL，並從同一個 Wi-Fi 網路開啟該網址重新設定。

## 安裝 v0.1.1

Release bundle 有繁體中文 `zh/` 與英文 `en/` 兩套。解壓 release binary ZIP 到來源目錄後，確認雜湊並先執行 dry-run：

```sh
(cd flash_images/esp32-s3-lcd-4b/releases/v0.1.1 && shasum -a 256 -c SHA256SUMS)
./write-rel.sh --dry-run
./write-rel-en.sh --dry-run
```

確認使用正確序列埠後，移除 `--dry-run` 執行所需語言的腳本。兩支腳本只更新 app 與字型 assets，保留 NVS 設定與歷史資料；一般更新不會寫入清空歷史資料的 image。`write.sh` 和 `write-en.sh` 也指向本次 v0.1.1 release。

### 完整燒錄（會清空整顆 Flash）

只有在要完整初始化裝置時才使用 `write-full.sh` 或 `write-full-en.sh`。這兩支腳本位於 v0.1.1 tag 之後的 `main`，固定使用 v0.1.1 的 zh/en bundle。先用目前 `main` 原始碼；再從 [v0.1.1 GitHub Release](https://github.com/superdhuang/powerwall-dashboard-esp32/releases/tag/v0.1.1) 下載 firmware ZIP，解壓到 repo 根目錄；或依下方建置步驟從 `v0.1.1` tag 產生 bundle，之後切回 `main`。預期目錄為 `flash_images/esp32-s3-lcd-4b/releases/v0.1.1/{zh,en}`。

先做 dry-run；預設序列埠是 `/dev/cu.usbmodem3101`，可用 `--port` 指定：

```sh
./write-full.sh --dry-run --port /dev/cu.usbmodem3101
./write-full-en.sh --dry-run --port /dev/cu.usbmodem3101
```

實際完整燒錄會清除整顆 16 MB Flash，包括 Wi-Fi/Gateway 憑證、裝置設定、NVS 與所有歷史資料。未加確認參數時，腳本會要求輸入 `ERASE ALL FLASH`；也可明確加 `--confirm-erase`：

```sh
./write-full.sh --port /dev/cu.usbmodem3101
./write-full-en.sh --port /dev/cu.usbmodem3101 --confirm-erase
```

每次執行都會寫入 `log/write-full*.log`。一般更新請使用上面的 `write-rel*.sh`，它們只更新 app/assets 並保留 NVS 與歷史資料。

## 從原始碼建置

需要 PlatformIO Core 與 Python 3。要重現 v0.1.1 韌體套件，先切到 release tag，再從 repo 根目錄執行：

```sh
git checkout v0.1.1
python3 firmware/tools/package_s3_release.py --release-id v0.1.1
```

此命令會建置 zh/en release image，產生 `flash_images/esp32-s3-lcd-4b/releases/v0.1.1/{zh,en}`，並在 manifest 記錄 app/assets 雜湊與來源 commit。若只要編譯 app image，可在 repo 根目錄分別執行：

```sh
pio run -d firmware -e waveshare-esp32s3-touch-lcd-4b-release
pio run -d firmware -e waveshare-esp32s3-touch-lcd-4b-en-release
```

`flash_images/` 由 `.gitignore` 排除；binary release ZIP 與 source repo 分開保存。

## Simulator（原始碼）

Git 原始碼只追蹤 simulator 原始碼，不包含編譯後的執行檔。v0.1.1 [GitHub Release](https://github.com/superdhuang/powerwall-dashboard-esp32/releases/tag/v0.1.1) 另提供可選的 macOS arm64 simulator ZIP；適用 macOS 26 以上，ZIP 不含 SDL2，請先用 Homebrew 安裝 `brew install sdl2`。從原始碼建置需 Apple Silicon Mac、Homebrew SDL2、PlatformIO Core 與 Python 3。首次 clone 後先安裝 PlatformIO 套件，再分別建置繁中與英文版：

```sh
pio pkg install -d firmware -e waveshare-esp32s3-touch-lcd-4b
make firmware_sim_native480
make firmware_sim_native480_en
./run-zh.sh --demo
./run-en.sh --demo
```

launcher 預設使用合成資料；demo 不連線 Gateway，也不讀取本機的 `nvs_config.json` 或歷史資料。要用本機設定進入 LIVE，先複製範本並填入自己的 Gateway IP 與密碼：

```sh
cp nvs_config.example.json nvs_config.json
./run-zh.sh --live
```

`nvs_config.json` 的格式如下；原始碼 repo 會忽略此檔，發佈套件不包含此檔：

```json
{
  "gateway_ip": "",
  "gateway_password": "",
  "gateway_email": "customer@example.com",
  "site_title": ""
}
```

也可以同時傳入 `--gateway-ip` 與 `--gateway-password`；兩者一起使用會直接進入 LIVE，並在記憶體中覆蓋檔案的 Gateway 欄位，不會寫回檔案。只提供其中一個會顯示錯誤並結束，程式不會回顯密碼：

```sh
./run-zh.sh --gateway-ip YOUR_GATEWAY_IP --gateway-password YOUR_GATEWAY_PASSWORD
```

請把示意值換成自己的設定。命令列密碼可能留在 shell history 或 process list；需要時改用本機 `nvs_config.json`。
