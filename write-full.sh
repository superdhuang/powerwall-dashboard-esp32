#!/bin/bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
log_dir="$script_dir/log"
mkdir -p "$log_dir"
log_file="$log_dir/write-full-$(date '+%Y%m%dT%H%M%S%z')-$$.log"
exec > >(tee -a "$log_file") 2>&1
printf 'Log file: %s\n' "$log_file"

usage() {
  cat <<'EOF'
Usage: ./write-full.sh [--dry-run] [--port PORT] [--confirm-erase]

Writes the pinned v0.1.2 Traditional Chinese full-flash bundle. A real run
requires --confirm-erase or typing the exact phrase ERASE ALL FLASH.
--dry-run validates the bundle and prints the erase/write plan without opening
the serial port or requiring confirmation.
EOF
}

dry_run=0
confirm_erase=0
args=()
for arg in "$@"; do
  case "$arg" in
    --help|-h)
      usage
      exit 0
      ;;
    --dry-run)
      dry_run=1
      args+=("$arg")
      ;;
    --confirm-erase)
      confirm_erase=1
      ;;
    --confirm-erase=*)
      printf 'Use --confirm-erase without a value.\n' >&2
      exit 2
      ;;
    --bundle|--bundle=*)
      printf 'This script is pinned to its v0.1.2 bundle; refusing %s.\n' "$arg" >&2
      exit 2
      ;;
    --update-only|--update-only=*)
      printf 'This is the full-flash script; --update-only is not accepted.\n' >&2
      exit 2
      ;;
    --full|--full=*)
      printf 'This script already selects full flash; do not pass %s.\n' "$arg" >&2
      exit 2
      ;;
    *)
      args+=("$arg")
      ;;
  esac
done

printf 'FULL FLASH WARNING: this erases the entire 16 MB flash, including Wi-Fi/Gateway credentials, device settings, NVS, and recorded history.\n'
if [[ "$dry_run" == 1 ]]; then
  printf 'Dry run only: no serial port will be opened and no flash will be erased or written.\n'
elif [[ "$confirm_erase" != 1 ]]; then
  if [[ ! -t 0 ]]; then
    printf 'Refusing a full flash without confirmation. Use --confirm-erase for an intentional destructive run.\n' >&2
    exit 2
  fi
  phrase=''
  printf 'Type exactly "ERASE ALL FLASH" to continue: '
  read -r phrase
  if [[ "$phrase" != 'ERASE ALL FLASH' ]]; then
    printf 'Confirmation did not match; no flash command was run.\n' >&2
    exit 2
  fi
fi

flash_args=(python3 "$script_dir/firmware/tools/flash_s3_assets.py" \
  --full --bundle "$script_dir/flash_images/esp32-s3-lcd-4b/releases/v0.1.2/zh")
if [[ ${#args[@]} -gt 0 ]]; then
  flash_args+=("${args[@]}")
fi
exec "${flash_args[@]}"
