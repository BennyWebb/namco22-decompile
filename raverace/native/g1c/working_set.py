#!/usr/bin/env python3
import pathlib,re,json,collections,struct
root=pathlib.Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g1c';s=(b/'attribution-clean/run.txt').read_text();courses={int(t):int(c) for t,c in re.findall(r'tick=(\d+) main=[^\n]*?major=(\d+)',s)};events=[(int(t),tuple(map(int,k.split(',')))) for t,k in re.findall(r'tick=(\d+) key=([\d,]+)',s)];groups=collections.defaultdict(set)
for t,k in events:groups[courses[t]].add(k)
out={'events':len(events),'unique':len(set(k for t,k in events)),'per_selector':{c:dict(unique=len(keys),padded_area_bytes=sum((k[4]+4)*(k[5]+4) for k in keys),grid8_area_bytes=sum(((k[4]+11)//8*8)*((k[5]+11)//8*8) for k in keys)) for c,keys in sorted(groups.items())},'interpretation':'Unique sums include transient/one-use scrolling rectangles; not simultaneous residency or a measured GPU requirement.'}
(b/'working-keys.bin').write_bytes(b''.join(struct.pack('<8i',t,courses[t],*k) for t,k in events));(root/'raverace/benchmarks/native-g1c-20261016/working-set.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
