#!/usr/bin/env python3
"""Prove a course source tag -> DSP object -> immutable mesh at tick 2500."""
import argparse,json,pathlib
import numpy as np
p=argparse.ArgumentParser();p.add_argument('capture',type=pathlib.Path);p.add_argument('geometry',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);a=p.parse_args()
b=(a.capture/'program.bin').read_bytes();ram=(a.capture/'wram.bin').read_bytes();poly=np.fromfile(a.capture/'poly.i32','<i4')
def read(buf,off,n):return int.from_bytes(buf[off:off+n],'big',signed=True)
course=6;segment=10;record=0xecdc+64*course;base=read(b,record+2,2);tag=base+2*segment+1;code=tag+0x45
assert base==1190 and tag==1211 and code==1280
assert read(ram,0xa0e8,2)==course and read(ram,0xa0c8,2)==segment
pair=read(b,record+8,4)+2*segment*8
camera=[read(ram,x,4) for x in [0xa128,0xa148,0xa168]]
position=[read(b,pair,4)-camera[0],-camera[1],read(b,pair+4,4)-camera[2]]
source=0x43a6;master=0x63a6;slave=0x7d1
assert poly[source:source+4].tolist()==[tag,*position]
assert int(poly[master])==tag
assert int(poly[slave])==code and int(poly[slave+1])==13
assert poly[master+1:master+4].tolist()==poly[slave+12:slave+15].tolist()
log=(a.capture/'run.log').read_text()
assert '[M1-MAP] pdp=2392 pc=4425 input=1211 output=1280' in log
model=json.loads((a.pack/f'model-{code}.json').read_bytes())
records=[json.loads(x) for x in a.geometry.read_text().splitlines()];assert records
cursor=0;checked_vertices=0
for emitted in records:
    found=False
    for j in range(cursor,len(model['quads'])):
        q=model['quads'][j]
        if q['color']!=emitted['color'] or q['flags']!=emitted['flags']:continue
        if q['material'][::2]!=[emitted['bank'],emitted['cmode']]:continue
        matches=True
        for i,actual in enumerate(emitted['vertices']):
            if not actual[6]:continue
            v=model['vertices'][q['first_vertex']+i]
            m=emitted['m'];t=emitted['t']
            xyz=[(sum(v[r]*m[3*r+c] for r in range(3))>>15)+t[c] for c in range(3)]
            mant,shift=emitted['zoom'];cx,cy=emitted['center']
            def div(n,d):return (abs(n)//abs(d))*(-1 if (n<0)!=(d<0) else 1)
            def sat(n):return max(-2147483648,min(2147483647,n))
            x=sat(cx*16+div((xyz[0]*mant*16)>>shift,xyz[2]))
            y=sat(cy*16-div((xyz[1]*mant*16)>>shift,xyz[2]))
            bri=v[5] if q['format']&64 else q['color']>>16
            if actual[:6]!=[x,y,xyz[2],v[3],v[4],bri]:matches=False;break
        if matches:
            cursor=j+1;found=True;checked_vertices+=sum(v[6] for v in emitted['vertices']);break
    assert found,f'no matching source quad for emitted geometry record {emitted}'
v=model['vertices'];result=dict(course=course,segment=segment,course_base_tag=base,source_tag=tag,
 dsp_pc='4425',dsp_opcode='CC45',object_code=code,source_record=hex(source),master_record=hex(master),slave_record=hex(slave),
 immutable_quads=len(model['quads']),emitted_quads=len(records),checked_vertices=checked_vertices,
 bounds_min=[min(x[k] for x in v) for k in range(3)],bounds_max=[max(x[k] for x in v) for k in range(3)],differences=0)
print(json.dumps(result,sort_keys=True))
