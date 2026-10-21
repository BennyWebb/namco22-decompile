#!/usr/bin/env python3
"""A killed supervisor must not leave its setup display/compiler/game child alive."""

import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

PLATFORM = Path(__file__).resolve().parents[2] / "platform/knulli-h700"
with tempfile.TemporaryDirectory() as directory:
    pidfile = Path(directory) / "child.pid"
    code = (
        "import sys,subprocess,pathlib,time; sys.path.insert(0,sys.argv[1]); "
        "from transport import child_guard; "
        'p=subprocess.Popen([sys.executable,"-c","import time; time.sleep(60)"],preexec_fn=child_guard); '
        "pathlib.Path(sys.argv[2]).write_text(str(p.pid)); time.sleep(60)"
    )
    parent = subprocess.Popen([sys.executable, "-c", code, str(PLATFORM), str(pidfile)])
    try:
        deadline = time.monotonic() + 5
        while not pidfile.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        assert pidfile.exists(), "child failed to start"
        child = int(pidfile.read_text())
        parent.kill()
        parent.wait()
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                state = Path(f"/proc/{child}/stat").read_text().rsplit(")", 1)[1].split()[0]
            except FileNotFoundError:
                break
            if state == "Z":
                break
            time.sleep(0.01)
        else:
            raise AssertionError("guarded child survived supervisor SIGKILL")
    finally:
        if parent.poll() is None:
            parent.kill()
            parent.wait()
print("PASS guarded child exits when supervisor is killed")
