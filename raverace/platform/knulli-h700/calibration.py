"""Generate calibrated EEPROM by replaying the ROM's TEST/SERVICE adjustment."""

import hashlib
import pathlib
import subprocess
import tempfile
import os
import shutil
from transport import child_guard

EXPECTED = "81e19e9ce2fca04dbbd487b12e85cc473e30e61f2797c703743c9b86373721c7"


def fnv(data):
    result = 14695981039346656037
    for byte in data:
        result = ((result ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return result


def generate(binary, roms, cache, output, env, log):
    output = pathlib.Path(output)
    for stale in output.parent.glob("calibration-*"):
        if stale.is_dir():
            shutil.rmtree(stale)
    with tempfile.TemporaryDirectory(prefix="calibration-", dir=output.parent) as directory:
        rec = pathlib.Path(directory) / "adjust.rec"
        rec.write_text(
            "F 1 FEFF 800 000 000\nF 1000 F2FF 800 000 000\n" "F 1100 FAFF 800 000 000\nF 1200 FEFF 800 000 000\n"
        )
        blank = bytes([255]) * 8192
        pathlib.Path(str(rec) + ".eeprom").write_bytes(blank)
        pathlib.Path(str(rec) + ".end").write_text("1600\n")
        # File identities, never an EEPROM image or captured game state.
        names = [
            "rv2_prguub.6d",
            "rv2_prgumb.8d",
            "rv2_prglmb.2d",
            "rv2_prgllb.4d",
            *[f"rv1cg{i}.{s}" for i, s in enumerate(["1a", "1c", "1d", "1e", "1f", "1j", "1k", "1n"])],
            "rv1ccrl.5a",
            "rv1ccrh.5c",
            *[f"rv1pot{s}{i}.{5-i}{e}" for s, e in [("l", "b"), ("m", "c"), ("u", "d")] for i in range(4)],
            "rv1data.6r",
            "rv1wav0.10r",
            "rv1wav2.10n",
            "rv1wav1.10p",
            "rv1wav3.10l",
            "rr1gam.2d",
            "rr1gam.3d",
            "rr1gam.4d",
            "rv1eeprm.9e",
        ]
        lines = [
            "RR_REPLAY 1",
            "GAME raverace",
            "TIMING 25600000/427350",
            "INPUT_BOUNDARY before-vblank",
            "RESET translated-factory-seed",
            "POLLS 83000",
            f"EEPROM {fnv(blank):016x}",
        ]
        for name in names:
            data = blank if name == "rv1eeprm.9e" else (pathlib.Path(roms) / name).read_bytes()
            lines.append(f"ROM {name} {len(data)} {fnv(data):016x}")
        pathlib.Path(str(rec) + ".meta").write_text("\n".join(lines) + "\n")
        temporary = pathlib.Path(directory) / "calibrated.nv"
        environment = dict(env, ENG_GLES3_FACTORY_EEPROM=str(temporary), ENG_GLES3_ASSETS=str(cache))
        environment.pop("ENG_GLES3_INITIAL_EEPROM", None)
        subprocess.run(
            [str(binary), str(roms), "--replay-input", str(rec)],
            env=environment,
            stdout=log,
            stderr=log,
            check=True,
            preexec_fn=child_guard,
            cwd=output.parent,
        )
        actual = hashlib.sha256(temporary.read_bytes()).hexdigest()
        if actual != EXPECTED:
            raise RuntimeError(f"EEPROM calibration failed: expected {EXPECTED}, got {actual}")
        with temporary.open("rb") as f:
            os.fsync(f.fileno())
        temporary.replace(output)
        os.sync()
    return output
