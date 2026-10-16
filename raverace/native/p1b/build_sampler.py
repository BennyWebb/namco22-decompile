#!/usr/bin/env python3
"""Build the observer with a 3MiB (not game's16MiB) output pool, no SHA warmup."""
import pathlib,json,subprocess
root=pathlib.Path(__file__).resolve().parents[3];src=root/'raverace/native/p1b';out=root/'raverace/native/generated/p1b/arm'
s=(src/'async_io.c').read_text().replace('#define POOL (16u*1024*1024)','#define POOL (3u*1024*1024)').replace('unsigned char warm[32];SHA256((const unsigned char*)"warm",4,warm);','')
assert '#define POOL (3u*1024*1024)' in s
(out/'observer_io.c').write_text(s)
cmd=json.loads((out/'build.json').read_text())['commands'][0];flags=cmd[:cmd.index('-c')]
commands=[flags+['-c',str(out/'observer_io.c'),'-o',str(out/'observer_io.o')],flags+[str(src/'memory_sampler.c'),str(out/'observer_io.o'),'-Wl,--wrap=fopen','-Wl,--wrap=fopen64','-lcrypto','-lpthread','-o',str(out/'memory_sampler')]]
for c in commands:subprocess.run(c,check=True)
(out/'sampler-build.json').write_text(json.dumps(dict(commands=commands,prefaulted_pool_bytes=3*1024*1024,interval_ms=100,pressure_reads=False),indent=2)+'\n')
