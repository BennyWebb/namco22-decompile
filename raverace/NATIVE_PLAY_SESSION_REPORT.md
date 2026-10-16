| Item | Result |
|---|---|
| User assessment | All four courses looked, felt and sounded perfect |
| Session | One real-play session, 22:50:25–23:04:07 BST (~13m42s); pipeline818.527s |
| Pictures | 48,876:48,714 native /162 legacy; all four selectors0–3 raced |
| Unsupported fallback | **0**, exact-key dictionary empty |
| Inferred repeats | 7 total:6 native +1 boot/prewarm; six events, one double |
| Settled audio shortage | 1,672 sample frames (34.833ms total at48kHz), no reported audible issue |
| Main gaps>50ms | One:50.270ms at tick42070, race selector2 |
| Race prep p99, selectors0/1/2/3 | 12.92 /12.94 /14.14 /14.24ms (upper0.01ms bins) |
| Exit / logs | Exit0; clocks restored;0 transport drops/errors/incomplete records |
| Production | SHA256 unchanged; all47 protected paths exact; clocks restored |

# First real Native play session

Only `20261016-225025-27784-297185128` follows the G2 scripted smoke.
Earlier22:32:05 setup attempt and22:36:53 corrected smoke are excluded; inventory
and their finish times are preserved in [evidence](benchmarks/native-play-20261016/README.md).
Interactive command has no replay/smoke injection. User observation is retained
without claiming the telemetry is anomaly-free or establishing all-format coverage.

State/course dwell below is **estimated** as pictures/59.7104Hz, excluding extra
repeats/startup/exit; early exact wall times rotated out. Raw main/sub IDs retained;
1=attract/start,2=selection,3=race;4/5=postrace states. Course names not inferred.
All59 main/sub/screen/course groups are in [state-course.csv](benchmarks/native-play-20261016/state-course.csv);
selection screens are combined below. Counts are complete, not reconstructed from tail logs.

| Main/sub | Course selector | Seconds≈ | Native | Legacy |
|---|---:|---:|---:|---:|
| 0/0 | 0 | 2.428 | 0 | 145 |
| 65535/0 | 0 | 0.033 | 0 | 2 |
| 65535/65535 | 65535 | 0.201 | 0 | 12 |
| 1/0 | 0 | 0.117 | 4 | 3 |
| 1/1 | 0 | 7.587 | 453 | 0 |
| 2/0 | 0 | 15.475 | 924 | 0 |
| 3/0 | 3 | 186.132 | 11114 | 0 |
| 3/0 | 0 | 196.934 | 11759 | 0 |
| 4/0 | 3 | 6.063 | 362 | 0 |
| 5/0 | 3 | 5.594 | 334 | 0 |
| 1/1 | 3 | 16.279 | 972 | 0 |
| 1/0 | 3 | 0.067 | 4 | 0 |
| 2/0 | 3 | 4.572 | 273 | 0 |
| 4/0 | 0 | 9.127 | 545 | 0 |
| 5/0 | 0 | 5.594 | 334 | 0 |
| 3/0 | 1 | 132.808 | 7930 | 0 |
| 4/0 | 1 | 6.063 | 362 | 0 |
| 5/0 | 1 | 5.627 | 336 | 0 |
| 1/1 | 1 | 2.763 | 165 | 0 |
| 1/0 | 1 | 0.067 | 4 | 0 |
| 2/0 | 1 | 9.295 | 555 | 0 |
| 3/0 | 2 | 175.028 | 10451 | 0 |
| 4/0 | 2 | 6.063 | 362 | 0 |
| 5/0 | 2 | 5.627 | 336 | 0 |
| 1/1 | 2 | 18.941 | 1131 | 0 |
| 1/0 | 2 | 0.067 | 4 | 0 |

All legacy reasons: `boot-state`159 (main0:145;main65535/sub0:2;
main65535/sub65535:12), `prewarm`3 (main1/sub0/course0).
Neither has an unsupported command key. Exact unsupported keys/counts: **none**.
Point-RAM, solid-polygon/object-flag and unknown-format fallbacks: **0 observed**.

| Repeat tick | Elapsed timestamp,seconds | State/course | Repeated vblanks |
|---|---:|---|---:|
| 160 | ≈2.680 | main=1 sub=0 screen=0 course=0 | 1 |
| 978 | ≈16.379 | main=3 sub=0 screen=0 course=3 | 1 |
| 994 | ≈16.647 | main=3 sub=0 screen=0 course=3 | 1 |
| 19484 | ≈326.308 | main=3 sub=0 screen=0 course=0 | 1 |
| 42068 | 704.531 | main=3 sub=0 screen=0 course=2 | 2 |
| 48556 | 813.201 | main=1 sub=1 screen=0 course=2 | 1 |

Exact times are monotonic swap return minus pipeline start52421.810237677;
early ≈ times are nominal tick/59.7104 estimates, not recovered timestamps.
Full16 events/service times retained in [events.json](benchmarks/native-play-20261016/events.json).
At42068, swap wait47.200ms accompanies two repeats;42070 follows at+704.564s,
main gap50.270ms, simulation queue-block39.401ms, prep7.460ms.
This supports presentation/queue backpressure; underlying stall cause is **unknown**.
Audio shortages: race selector2=1214,attract selector2=373,postrace selector0=85.
At978/994 prep18.917/15.409ms; other repeat causes are not proven.

Minimum MemAvailable428.523MiB,CmaFree2.3125MiB; peak Mali186.809MiB,
RSS333.125MiB (HWM333.129). Peak SoC65.186°C; CPU/GPU cooling states always0.
All8203 sampled clocks:CPU1512MHz,GPU240MHz;0 clock/cooling flags.
Summary/clock journal confirm restoration without errors; current values checked below.
Native EEPROM persisted once; current SHA256
`c713a3e161c5aa79408401283a213b363d23a3e499918e903b1da0a19731f310`
differs from G2 seed-copy `a34d3b84…`. No save contents copied; production saves untouched.

Production SHA256 `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`; all47protected paths pass.
Current clocks equal saved schedutil480000–1512000/GPUstock bounds0/0; EmulationStation running,no game.
Read-only retrieval/hash verification only; copied files match device hashes; no commit/push.

Issues found:
- Rare pacing/audio anomalies above, despite the user's excellent subjective result.
- Detailed THREAD/SIM profiler capacity overflow40684/40685 at8192 records;
  transport drops/errors0,unmatched records0,fallback-key overflow0,no state-group overflow.
  Full-session counts/p99 histograms survive; raw profile retains only41123–48876.
- Log rotation prevents exact early anomaly timestamps; no full-run syscall/stack attribution.
Recommendation: retain this playable build; if separately authorised, attribute the
rare long-swap/queue event and improve long-session diagnostics. No fixes started.
**STOPPED.**
