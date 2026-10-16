# C1 host design audit — BLOCKED / STOPPED

See [report](../../NATIVE_C1_REPORT.md) and
[audit source](../../native/c1/audit_flattening.py).
`flattening-audit.json` records source texture ROM identities, every full expanded
bank hash, recipe inventory, exact cross-bank counterexample, payload arithmetic
and scalar decoder checks. No ROM bytes, images or generated texture cache are
published. No shader or game build was modified; no hardware replay was run.

All 16 map planes differ; reference recipes span 15 banks. Direct flattening
requires 256 MiB full-map / 240 MiB reference before palette, not 16 MiB.
The reference reduction is only 1.909×, failing the 10× goal for that layout.
This is a design blocker, not a GPU performance result. Read the report's
unperformed-gates section before interpreting the audit as validation.

`production-check.txt` records the read-only final hardware integrity check.
`repository.json` binds starting HEAD and audit inputs/source. No commits or push.
Further representation design requires user authorisation.
