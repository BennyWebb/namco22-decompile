"""Shared asynchronous child output drain for full and summary sessions."""

import os
import selectors
import time


import ctypes
import signal

LIBC = ctypes.CDLL(None, use_errno=True)


def child_guard():
    LIBC.prctl(1, signal.SIGKILL, 0, 0, 0)
    if os.getppid() == 1:
        os._exit(96)


def persist_eeprom(path, data):
    if len(data) != 8192:
        raise RuntimeError("Save write failed: invalid EEPROM size.")
    temporary = path.with_suffix(".tmp")
    with temporary.open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)
    directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)
    if path.read_bytes() != data:
        raise RuntimeError(f"Save readback failed: {path}")


def monitor(child, profile, collector, *, ack=None):
    sel = selectors.DefaultSelector()
    buffers = {}
    for kind, fd in [("stderr", child.stderr.fileno()), ("stdout", child.stdout.fileno()), ("profile", profile)]:
        os.set_blocking(fd, False)
        sel.register(fd, selectors.EVENT_READ, kind)
        buffers[fd] = b""
    next_sample = time.monotonic()
    exit_time = None
    while sel.get_map():
        for key, mask in sel.select(0.05):
            data = os.read(key.fd, 65536)
            if not data:
                if buffers[key.fd]:
                    collector.line(key.data, buffers[key.fd].decode(errors="replace"))
                sel.unregister(key.fd)
                continue
            buffers[key.fd] += data
            while b"\n" in buffers[key.fd]:
                line, buffers[key.fd] = buffers[key.fd].split(b"\n", 1)
                decoded = line.decode(errors="replace") + "\n"
                collector.line(key.data, decoded)
                if key.data == "stderr" and decoded.startswith("[SAVE-FLUSH] "):
                    if ack is None or decoded != "[SAVE-FLUSH] return-title\n":
                        raise RuntimeError("Invalid save acknowledgement request")
                    if os.write(ack, b"OK return-title\n") != len(b"OK return-title\n"):
                        raise RuntimeError("Save acknowledgement failed")
            if len(buffers[key.fd]) > 1048576:
                collector.unmatched += 1
                buffers[key.fd] = b""
        now = time.monotonic()
        if now >= next_sample:
            collector.sample(child.pid)
            next_sample += 0.1
            if next_sample < now - 0.1:
                next_sample = now + 0.1
        if child.poll() is not None:
            if exit_time is None:
                exit_time = now
            if now - exit_time > 5:
                collector.unmatched += len(sel.get_map())
                break
    sel.close()
    return child.wait()
