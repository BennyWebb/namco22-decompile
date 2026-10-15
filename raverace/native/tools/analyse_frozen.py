#!/usr/bin/env python3
"""Summarise isolated frozen-scene ceiling measurements and emit comparisons."""
import argparse,json,pathlib,re,statistics,struct,zlib
p=argparse.ArgumentParser();p.add_argument('results',type=pathlib.Path);p.add_argument('scene',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
reports=[]
reference=(a.scene/'reference.rgba').read_bytes()
assert len(reference)==640*480*4
for folder in sorted(a.results.glob('run*')):
 text=(folder/'run.log').read_text();modes={}
 for mode in range(3):assert (folder/f'mode-{mode}.rgba').read_bytes()==reference
 for mode in range(3):
  rows=[list(map(float,m[1:])) for m in re.findall(r'TIMING sample=\d+ mode=(\d+) cpu_ms=([\d.]+) submission_ms=([\d.]+) completion_ms=([\d.]+)',text) if int(m[0])==mode]
  assert len(rows)==30
  modes[mode]={key:dict(median=statistics.median(v),p95=sorted(v)[int(.95*(len(v)-1))],maximum=max(v)) for key,v in zip(['thread_cpu_ms','submission_ms','completion_ms'],zip(*rows))}
 assert len(re.findall(r'VERIFY mode=\d vs_original=0 vs_capture=0',text))==3
 reports.append(dict(run=folder.name,modes=modes,rss_kib=int(re.search(r'RSS max_kib=(\d+)',text)[1])))
report=dict(runs=reports,median_of_round_medians={mode:{metric:statistics.median(r['modes'][mode][metric]['median'] for r in reports) for metric in ['thread_cpu_ms','submission_ms','completion_ms']} for mode in range(3)},scope='Frozen world pass only; completion includes command submission and glFinish, not exclusive GPU time or playable FPS.')
(a.out/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['median_of_round_medians'],indent=2))
def png(path,b,w=640,h=480):
 def ch(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d)&0xffffffff)
 scan=b''.join(b'\0'+b[i*w*4:(i+1)*w*4] for i in range(h))
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+ch(b'IDAT',zlib.compress(scan))+ch(b'IEND',b''))
def flip(b):return b''.join(b[y*640*4:(y+1)*640*4] for y in range(479,-1,-1))
base=flip((a.results/'run1/mode-0.rgba').read_bytes());native=flip((a.results/'run1/mode-2.rgba').read_bytes());assert base==native
# Priority alpha is compositor metadata, so show RGB with opaque alpha in previews.
base=bytearray(base);native=bytearray(native)
base[3::4]=bytes([255])*(640*480);native[3::4]=bytes([255])*(640*480)
png(a.out/'incumbent-world.png',base);png(a.out/'resident-world.png',native)
side=b''.join(base[i*2560:(i+1)*2560]+native[i*2560:(i+1)*2560] for i in range(480));png(a.out/'side-by-side.png',side,1280)
