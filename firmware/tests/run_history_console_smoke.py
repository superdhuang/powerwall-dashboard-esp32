#!/usr/bin/env python3
"""Exercise real SDL/stdin history writes in isolated directories, with no live Gateway."""
import datetime, json, os, pathlib, subprocess, tempfile, time
root = pathlib.Path(__file__).resolve().parents[2]
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', TZ='Asia/Taipei', NO_COLOR='1',
           HTTPS_PROXY='http://127.0.0.1:1', HTTP_PROXY='http://127.0.0.1:1', ALL_PROXY='http://127.0.0.1:1')

def wait_for(proc, log, text, count=1):
    until = time.monotonic() + 25
    while time.monotonic() < until:
        output = log.read_text(errors='replace')
        if output.count(text) >= count: return output
        if proc.poll() is not None: raise AssertionError(f'early exit {proc.returncode}: {output[-1200:]}')
        time.sleep(.05)
    raise AssertionError(f'timed out waiting {text}: {log.read_text(errors="replace")[-1400:]}')

def run(binary, demo=False):
    with tempfile.TemporaryDirectory(prefix='pw-history-key-') as name:
        work = pathlib.Path(name)
        date = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).date().isoformat()
        (work/'nvs_config.json').write_text(json.dumps({'gateway_ip':'127.0.0.1:1','gateway_password':'fixture','gateway_email':'fixture@example.invalid'}))
        fixture = {'date':date, 'points':[{'hour':0,'soe':61,'load_kw':1.5,'power_source':'grid'}]}
        (work/'history_today.json').write_text(json.dumps(fixture))
        log = work/'console.log'
        with log.open('w') as out:
            proc = subprocess.Popen([str(root/binary)] + (['--demo'] if demo else []), cwd=work, env=env, stdin=subprocess.PIPE, stdout=out, stderr=subprocess.STDOUT)
            try:
                wait_for(proc, log, 'no Enter needed')
                original = (work/'history_today.json').read_bytes()
                proc.stdin.write(b'6'); proc.stdin.flush()
                if demo:
                    wait_for(proc, log, 'SKIPPED: DEMO active')
                    assert (work/'history_today.json').read_bytes() == original
                    assert not (work/'history_week.json').exists()
                else:
                    wait_for(proc, log, 'source=manual target=weekly END OK')
                    today = work/'history_today.json'; week = work/'history_week.json'
                    first_mtimes = (today.stat().st_mtime_ns, week.stat().st_mtime_ns)
                    saved = json.loads(today.read_text())
                    assert saved['points'][0]['soe'] == 61
                    proc.stdin.write(b'6'); proc.stdin.flush()
                    wait_for(proc, log, 'source=manual target=weekly END OK', 2)
                    assert today.stat().st_mtime_ns > first_mtimes[0] and week.stat().st_mtime_ns > first_mtimes[1], 'unchanged histories must really be rewritten'
                    # Make only the week's temporary file path unwritable as a directory.
                    blocked = work/f'history_week.json.{proc.pid}.tmp'; blocked.mkdir()
                    proc.stdin.write(b'6'); proc.stdin.flush()
                    wait_for(proc, log, 'source=manual target=weekly END FAILED')
                    blocked.rmdir()
                    proc.stdin.write(b'6'); proc.stdin.flush()
                    output = wait_for(proc, log, 'source=manual target=weekly END OK', 3)
                    manual = [line for line in output.splitlines() if 'source=manual' in line]
                    assert len(manual) == 16, manual
                    assert not any('source=auto' in line for line in output.splitlines()), 'manual keys must not trigger an extra poll/save'
                proc.stdin.write(b'\x1b'); proc.stdin.flush()
                proc.wait(timeout=12)
                assert proc.returncode == 0, proc.returncode
            finally:
                if proc.poll() is None: proc.kill(); proc.wait()
        destination = pathlib.Path(tempfile.gettempdir())/f'{binary}-history-console-{"demo" if demo else "live"}.log'
        destination.write_bytes(log.read_bytes())
        print(f'PASS {binary} {"DEMO write guard" if demo else "stdin 6 (no Enter), unchanged actual rewrites, failure + retry"}: {destination}', flush=True)

for binary in ('firmware_sim', 'firmware_sim_480'):
    run(binary)
    run(binary, True)
