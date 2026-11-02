#!/usr/bin/env python3
import subprocess
import sys
import tempfile


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="powerwall-history-v1-") as directory:
        return subprocess.run([sys.argv[1]], cwd=directory, check=False).returncode


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: run_daily_history_store_v1_smoke.py <test-binary>")
    raise SystemExit(main())
