#!/bin/bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
log_dir="$script_dir/log"
mkdir -p "$log_dir"
log_file="$log_dir/write-rel-en-$(date '+%Y%m%dT%H%M%S%z')-$$.log"
exec > >(tee -a "$log_file") 2>&1
printf 'Log file: %s\n' "$log_file"
for arg in "$@"; do
  case "$arg" in
    --full|--full=*|--bundle|--bundle=*)
      printf 'This release script only allows updates that preserve NVS; refusing %s.\n' "$arg" >&2
      exit 2
      ;;
  esac
done
# Pin the English writer to the tagged v0.1.1 release bundle.
exec python3 "$script_dir/firmware/tools/flash_s3_assets.py" \
  --update-only --bundle "$script_dir/flash_images/esp32-s3-lcd-4b/releases/v0.1.1/en" "$@"
