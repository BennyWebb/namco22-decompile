"""Bounded GLES3-only state, pacing and failure summaries."""

import pathlib, json, math, re, csv, collections, array, time, hashlib, os, gzip

T = 1 / json.loads(pathlib.Path(__file__).with_name("device-profile.json").read_text())["panel_hz"]
SETTLED_TICK = 180


class Rolling:
    def __init__(self, path, limit=2 * 1024 * 1024):
        self.path = path
        self.limit = limit
        self.size = 0
        self.rotations = 0
        self.f = path.open("wb", buffering=65536)

    def write(self, data):
        if self.size + len(data) > self.limit:
            self.f.close()
            os.replace(self.path, self.path.with_name(self.path.name + ".1"))
            self.f = self.path.open("wb", buffering=65536)
            self.size = 0
            self.rotations += 1
        self.f.write(data[: self.limit])
        self.size += min(len(data), self.limit)

    def close(self):
        self.f.close()


class Retained:
    """Lossless compressed diagnostic stream; never rotates out an event window."""

    def __init__(self, path):
        self.f = gzip.open(path, "wb", compresslevel=1)

    def write(self, data):
        self.f.write(data)

    def close(self):
        self.f.close()


class Group:
    def __init__(self):
        self.gles3 = self.repeats = self.underruns = self.all_audio = self.gaps = self.misses = self.evictions = (
            self.copied
        ) = self.clock_flags = 0
        self.hist = array.array("I", [0]) * 10002
        self.prep_worst = 0
        self.resources = {}

    def add(self, row, prev):
        tick = int(row["frame"])
        self.gles3 += 1
        ms = float(row["prep_wall"]) * 1000
        self.hist[min(10001, math.ceil(ms * 100))] += 1
        self.prep_worst = max(self.prep_worst, ms)
        if not prev:
            return None
        dt = float(row["presented"]) - float(prev["presented"])
        repeat = max(0, math.floor(dt / T + 0.5) - 1)
        under = int(row["audio_underruns"]) - int(prev["audio_underruns"])
        gap = (
            float(row["presented"])
            - float(row["publication_age"])
            - float(prev["presented"])
            + float(prev["publication_age"])
        )
        self.repeats += repeat
        self.all_audio += under
        self.underruns += under if tick > SETTLED_TICK else 0
        self.gaps += gap > 0.05
        if repeat or under or gap > 0.05:
            return dict(
                tick=tick,
                repeats=repeat,
                underrun_frames=under,
                main_gap_ms=gap * 1000,
                prepare_ms=ms,
                compiler_ms=float(row["gles3_compile_ms"]),
                swap_ms=float(row["swap_wall"]) * 1000,
            )

    def summary(self):
        rank = math.ceil(self.gles3 * 0.99)
        acc = 0
        p99 = None
        for i, n in enumerate(self.hist):
            acc += n
            if rank and acc >= rank:
                p99 = ">100ms" if i == 10001 else i / 100
                break
        return dict(
            gles3=self.gles3,
            repeats=self.repeats,
            settled_underrun_frames=self.underruns,
            all_underrun_frames=self.all_audio,
            main_gaps_over50ms=self.gaps,
            preparation_p99_ms_upper_0_01=p99,
            preparation_worst_ms=self.prep_worst,
            dynamic_misses=self.misses,
            dynamic_evictions=self.evictions,
            staging_copied_bytes=self.copied,
            resources=self.resources,
            clock_flagged_samples=self.clock_flags,
        )


class Collector:
    def __init__(self, run, save, retention=None):
        self.retention = retention
        self.trigger_count = 0
        self.trigger_file = None
        self.replay_completed = None
        if retention:
            retention.mkdir()
            self.trigger_file = (retention / "events.jsonl").open("w")
        self.run = run
        self.save = save
        self.logs = {
            n: (Retained(retention / (n + ".log.gz")) if retention else Rolling(run / (n + ".log")))
            for n in ["stderr", "stdout", "profile", "resources", "problems"]
        }
        self.groups = {}
        self.pending = {}
        self.next = 1
        self.prev = None
        self.header = None
        self.current = "unknown"
        self.events = collections.deque(maxlen=256)
        self.unmatched = self.save_writes = self.samples = 0
        self.io = []
        self.smoke_chord = False
        self.unsupported = None

    def group(self, key):
        if key not in self.groups:
            if len(self.groups) >= 127:
                key = "overflow"
            if key not in self.groups:
                self.groups[key] = Group()
        return self.groups[key]

    def problem(self, obj):
        self.events.append(obj)
        self.logs["problems"].write((json.dumps(obj) + "\n").encode())

    def line(self, kind, line):
        if kind == "stderr" and line.startswith("[G2-EEPROM] "):
            data = bytes.fromhex(line.split(" ", 1)[1].strip())
            assert len(data) == 8192
            temp = self.save.with_suffix(".new")
            with temp.open("wb") as f:
                f.write(data)
                f.flush()
                os.fsync(f.fileno())
            os.replace(temp, self.save)
            self.save_writes += 1
            line = "[G2-EEPROM] persisted bytes=8192 sha256=" + hashlib.sha256(data).hexdigest() + "\n"
        self.logs[kind].write(line.encode(errors="replace"))
        if kind == "profile":
            values = next(csv.reader([line]))
            if values and values[0] == "frame":
                self.header = values
                return
            if not self.header or len(values) != len(self.header):
                self.unmatched += 1
                return
            row = dict(zip(self.header, values))
            self.pending.setdefault(int(row["frame"]), {})["row"] = row
            if self.trigger_file:
                reasons = []
                if int(row.get("draws", 0)) > 500:
                    reasons.append("draws>500")
                if float(row["prep_wall"]) > 0.0167:
                    reasons.append("preparation>16.7ms")
                if float(row.get("cpu", 0)) > 0.0167:
                    reasons.append("render_cpu>16.7ms")
                if reasons:
                    self.trigger_count += 1
                    self.trigger_file.write(json.dumps(dict(tick=int(row["frame"]), reasons=reasons, row=row)) + "\n")
        elif kind == "stderr":
            completed = re.search(r"\[REPLAY\] completed_frames=(\d+) status=(\w+)", line)
            if completed:
                self.replay_completed = int(completed[1]) if completed[2] == "ok" else None
            if line.startswith("GLES3 UNSUPPORTED:"):
                self.unsupported = line.strip()
                self.problem(dict(unsupported=self.unsupported))
            m = re.search(
                r"\[G1-COVERAGE\] tick=(\d+) main=(\d+) sub=(\d+) screen=(\d+) course=(\d+) major=(\d+) walk=\d+ direct=\d+ gles3=(\d+) reason=(.*)",
                line,
            )
            if m:
                t, main, sub, screen, route, course, gles3, reason = m.groups()
                assert gles3 == "1"
                key = f"main={main} sub={sub} screen={screen} course={course}"
                self.current = key
                self.pending.setdefault(int(t), {})["key"] = key
            m = re.search(r"G2-STAGING\] tick=(\d+) copied_bytes=(\d+)", line)
            if m:
                self.pending.setdefault(int(m[1]), {})["copied"] = int(m[2])
            m = re.search(r"G1B-PREP\] tick=(\d+) hits=\d+ misses=(\d+) evictions=(\d+)", line)
            if m:
                self.pending.setdefault(int(m[1]), {})["dynamic"] = (int(m[2]), int(m[3]))
            if "[P1B-IO]" in line:
                self.io.append(line.strip())
            if "[G2-SMOKE] START+SELECT" in line:
                self.smoke_chord = True
        while self.next in self.pending and "row" in self.pending[self.next] and "key" in self.pending[self.next]:
            p = self.pending.pop(self.next)
            row = p["row"]
            g = self.group(p["key"])
            g.copied += p.get("copied", 0)
            d = p.get("dynamic", (0, 0))
            g.misses += d[0]
            g.evictions += d[1]
            event = g.add(row, self.prev)
            if event:
                self.problem(dict(state=p["key"], **event))
            self.prev = row
            self.next += 1
        if len(self.pending) > 8192:
            first = min(self.pending)
            self.pending.pop(first)
            self.unmatched += 1
            if first == self.next:
                self.next += 1

    def sample(self, pid):
        self.samples += 1
        g = self.group(self.current)
        v = {}
        root = pathlib.Path("/sys")
        try:
            status = pathlib.Path(f"/proc/{pid}/status").read_text()
            mem = pathlib.Path("/proc/meminfo").read_text()
            for key, text in [("VmRSS", status), ("VmHWM", status), ("MemAvailable", mem), ("CmaFree", mem)]:
                m = re.search(r"^" + key + r":\s+(\d+)", text, re.M)
                if m:
                    v[key + "_KiB"] = int(m[1])
            mali = (root / "kernel/debug/mali0/gpu_memory").read_text()
            m = re.search(r"^mali0\s+(\d+)", mali, re.M)
            if m:
                v["Mali_pages"] = int(m[1])
            for i in range(4):
                v[f"cpu{i}_khz"] = int((root / f"devices/system/cpu/cpu{i}/cpufreq/scaling_cur_freq").read_text())
            v["gpu_hz"] = int((root / "class/devfreq/gpu/cur_freq").read_text())
            for p in (root / "class/thermal").glob("thermal_zone*/temp"):
                v[p.parent.name + "_mC"] = int(p.read_text())
            for p in (root / "class/thermal").glob("cooling_device*/cur_state"):
                v[p.parent.name] = int(p.read_text())
            if (
                any(v.get(f"cpu{i}_khz") != 1512000 for i in range(4))
                or v.get("gpu_hz") != 240000000
                or any(v.get("cooling_device" + str(i), 0) for i in range(2))
            ):
                g.clock_flags += 1
        except (OSError, ValueError):
            v["read_error"] = 1
        for key, value in v.items():
            item = g.resources.setdefault(key, dict(min=value, max=value))
            item["min"] = min(item["min"], value)
            item["max"] = max(item["max"], value)
        if self.samples % 10 == 0:
            self.logs["resources"].write(
                (json.dumps(dict(monotonic=time.monotonic(), state=self.current, values=v)) + "\n").encode()
            )

    def finish(self, exit_code, restore_errors, extra):
        out = dict(
            exit_code=exit_code,
            clock_restore_errors=restore_errors,
            groups={k: g.summary() for k, g in self.groups.items()},
            processed_pictures=self.next - 1,
            unmatched_records=self.unmatched + len(self.pending),
            unsupported=self.unsupported,
            events=list(self.events),
            save_writes=self.save_writes,
            io_receipts=self.io,
            smoke_start_select_seen=self.smoke_chord,
            sample_count=self.samples,
            settled_audio_start_tick=SETTLED_TICK + 1,
            **extra,
        )
        if self.retention:
            out.update(
                diagnostic_retention=str(self.retention),
                diagnostic_triggers=self.trigger_count,
                retention_policy="complete compressed streams; no rolling eviction",
            )
        text = [
            self.unsupported or "Rave Racer GLES3-only session",
            f"Exit: {exit_code}; clocks restored: {not restore_errors}",
            f'Pictures: {self.next-1}; incomplete records: {out["unmatched_records"]}',
        ]
        for key, g in out["groups"].items():
            text.append(
                f'{key}: gles3={g["gles3"]} repeats={g["repeats"]} settled_underruns={g["settled_underrun_frames"]} main_gaps50={g["main_gaps_over50ms"]} prep_p99_ms={g["preparation_p99_ms_upper_0_01"]}'
            )
        text.extend(self.io)
        (self.run / "summary.json").write_text(json.dumps(out, indent=2) + "\n")
        (self.run / "summary.txt").write_text("\n".join(text) + "\n")
        for f in self.logs.values():
            f.close()
        if self.trigger_file:
            self.trigger_file.close()
        return out
