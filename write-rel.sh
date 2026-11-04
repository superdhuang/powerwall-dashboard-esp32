#!/bin/bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
log_dir="$script_dir/log"
mkdir -p "$log_dir"
log_file="$log_dir/write-rel-$(date '+%Y%m%dT%H%M%S%z')-$$.log"
exec > >(tee -a "$log_file") 2>&1
printf 'Log file: %s\n' "$log_file"
for arg in "$@"; do
  case "$arg" in
    --full|--full=*|--bundle|--bundle=*)
      printf '這支 release 腳本只允許保留 NVS 的更新；不接受 %s。\n' "$arg" >&2
      exit 2
      ;;
  esac
done
# Pin the Traditional Chinese writer to the tagged v0.1.2 release bundle.
exec python3 "$script_dir/firmware/tools/flash_s3_assets.py" \
  --update-only --bundle "$script_dir/flash_images/esp32-s3-lcd-4b/releases/v0.1.2/zh" "$@"
