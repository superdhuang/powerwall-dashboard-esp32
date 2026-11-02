#!/bin/bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir"

live=0
sim_args=()
for arg in "$@"; do
  case "$arg" in
    --live)
      live=1
      ;;
    --gateway-ip|--gateway-password|--gateway-ip=*|--gateway-password=*)
      live=1
      sim_args+=("$arg")
      ;;
    *)
      sim_args+=("$arg")
      ;;
  esac
done

if [[ "$live" == 1 ]]; then
  live_args=()
  for arg in "${sim_args[@]}"; do
    [[ "$arg" == --demo ]] || live_args+=("$arg")
  done
  exec "$script_dir/firmware_sim_native480_en" "${live_args[@]}"
fi

exec "$script_dir/firmware_sim_native480_en" --demo "${sim_args[@]}"
