#!/usr/bin/env python3
"""Audit controller-only fixtures and mandatory SHA checks, including under -O."""

from pathlib import Path
import re
import tempfile
import unittest

from recordings import HERE, IDENTITIES, prepare_recording, sha


class ControllerInputs(unittest.TestCase):
    def test_content(self):
        for name, identity in IDENTITIES.items():
            with self.subTest(name=name):
                path = HERE / "recordings" / (name + ".rec")
                self.assertEqual(sha(path), identity["input_sha256"])
                previous = 0
                for line in path.read_text(encoding="ascii").splitlines():
                    if line.startswith("#"):
                        self.assertTrue(
                            line == "# Rave Racer input recording: F frame inputs steer gas brake (hex), on change"
                            or re.fullmatch(r"# MARK \d+ \d+", line)
                        )
                        continue
                    match = re.fullmatch(r"F (\d+) [0-9A-F]{4} [0-9A-F]{3} [0-9A-F]{3} [0-9A-F]{3}", line)
                    self.assertIsNotNone(match)
                    tick = int(match[1])
                    self.assertGreater(tick, previous)
                    self.assertLessEqual(tick, identity["ticks"])
                    previous = tick
                metadata = Path(str(path) + ".meta").read_text(encoding="ascii")
                for line in metadata.splitlines():
                    self.assertRegex(
                        line,
                        r"^(RR_REPLAY 1|GAME raverace|TIMING 25600000/427350|"
                        r"INPUT_BOUNDARY before-vblank|RESET translated-factory-seed|POLLS 83000|"
                        r"EEPROM [0-9a-f]{16}|ROM [a-z0-9_.]+ [0-9]+ [0-9a-f]{16})$",
                    )
                self.assertEqual(Path(str(path) + ".end").read_text().strip(), str(identity["ticks"]))
                self.assertFalse(Path(str(path) + ".eeprom").exists())

    def test_corrupt_override_is_rejected(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            (root / "reference.rec").write_text("F 1 FFFF 800 000 000\n")
            with self.assertRaisesRegex(RuntimeError, "Input recording SHA256 mismatch"):
                prepare_recording("reference", root, root / "output", root / "missing.eeprom")
            self.assertFalse((root / "output").exists())


if __name__ == "__main__":
    unittest.main()
