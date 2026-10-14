#!/usr/bin/env python3
"""Offline Rave Racer asset compiler/inspector. No emulator integration."""
import argparse
import hashlib
import json
import pathlib
import sys
import numpy as np

VERSION = 1

def digest(data):
    return hashlib.sha256(data).hexdigest()

def encoded(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n').encode()

def require(condition, message):
    if not condition:
        raise ValueError(message)

class ROM:
    def __init__(self, root):
        self.sources = {}
        def read(name, size):
            data = (root / name).read_bytes()
            require(len(data) == size, f'{name}: expected {size} bytes, got {len(data)}')
            self.sources[name] = dict(bytes=size, sha256=digest(data))
            return data
        planes = [np.frombuffer(b''.join(read(f'rv1pot{s}{i}.{5-i}{e}', 0x80000)
                   for i in range(4)), np.uint8).astype(np.int32)
                  for s, e in [('l', 'b'), ('m', 'c'), ('u', 'd')]]
        w = planes[0] | planes[1] << 8 | planes[2] << 16
        self.words = (w ^ 0x800000) - 0x800000
        self.cg = np.frombuffer(b''.join(read(f'rv1cg{i}.{s}', 0x200000)
                         for i, s in enumerate(['1a','1c','1d','1e','1f','1j','1k','1n'])), np.uint8)
        self.tiles = np.frombuffer(read('rv1ccrl.5a', 0x200000), '<u2').astype(np.uint32)
        nib = np.frombuffer(read('rv1ccrh.5c', 0x80000), np.uint8)
        self.attr = np.empty(0x100000, np.uint8)
        self.attr[::2], self.attr[1::2] = nib >> 4, nib & 15
        self.tiles |= (self.attr.astype(np.uint32) & 1) << 16

    def word(self, address):
        require(0 <= address < len(self.words), f'point address outside ROM: {address:#x}')
        return int(self.words[address])

    def model(self, code):
        cursor = self.word(code)
        quads, packets, vertices, indices = [], [], [], []
        for _ in range(4096):
            chunk = self.word(cursor); cursor += 1
            if chunk < 0:
                require(chunk == -1, f'object {code}: unexpected terminator {chunk}')
                return dict(code=code, list_address=self.word(code), vertices=vertices,
                            indices=indices, quads=quads, lighting_packets=packets)
            size = self.word(chunk)
            require(0 <= size <= 256, f'object {code}: invalid chunk length {size}')
            pos, end = chunk + 1, chunk + 1 + size
            while pos < end:
                length = self.word(pos); base = pos + 1
                require(length in (23,24,16,13), f'object {code}: unknown packet {length} at {pos:#x}')
                require(base + length <= end, f'object {code}: truncated packet at {pos:#x}')
                raw = [self.word(base+i) for i in range(length)]
                if length in (23,24):
                    offset = 3 if length == 23 else 4
                    uv = [x & 0xffffff for x in raw[offset:offset+8]]
                    xyz = raw[offset+8:offset+20]
                    first = len(vertices)
                    vertices.extend([xyz[i*3:i*3+3] + [uv[i*2]&4095, uv[i*2+1]&4095,
                                     (uv[i]>>16)&255] for i in range(4)])
                    indices.extend([first,first+1,first+2,first,first+2,first+3])
                    quads.append(dict(packet_address=pos, format=raw[0]&0xffffff,
                        flags=raw[1]&0xffffff, color=raw[2]&0xffffff,
                        polyshift=raw[3]&0xffffff if length==24 else 0,
                        uv_raw=uv, first_vertex=first, lighting_packet_count=len(packets),
                        material=[(uv[1]>>12)&15,(raw[2]>>8)&127,(uv[0]>>12)&15]))
                else:
                    packets.append(dict(address=pos,length=length,raw=raw,
                                        reset=length==16, normals=raw[4:] if length==16 else raw[1:]))
                pos = base + length
                if raw[0] & 0x800000:
                    break  # same early-end semantics as blit_quads
        raise ValueError(f'object {code}: unterminated list')

    def pens(self, bank, cmode, u0, v0, width, height):
        u = (u0 + np.arange(width, dtype=np.uint32)) & 4095
        v = ((v0 + np.arange(height, dtype=np.uint32)) & 4095) | (bank * 4096)
        ix = ((v[:,None]&0xfff0)<<4) | ((u[None,:]&0xff0)>>4)
        tile, at = self.tiles[ix], self.attr[ix]
        x = np.broadcast_to(u[None,:]&15, ix.shape); y = np.broadcast_to(v[:,None]&15, ix.shape)
        x = np.where(at&4,15-x,x); y = np.where(at&2,15-y,y)
        xx, yy = np.where(at&8,y,x), np.where(at&8,x,y)
        off = tile*256+yy*16+xx
        raw = np.where(off<len(self.cg),self.cg[np.minimum(off,len(self.cg)-1)],0).astype(np.uint32)
        if cmode&4: shift,mask,offset=2*(~cmode&3),3,0xec+((cmode&8)<<1)
        elif cmode&2: shift,mask,offset=4*(~cmode&1),15,0xe0+((cmode&8)<<1)
        else: shift,mask,offset=0,255,0
        return ((offset+((raw>>shift)&mask))&255).astype(np.uint8)

def validate_model(m):
    v, ix = m['vertices'], m['indices']
    require(len(v)==4*len(m['quads']) and len(ix)==6*len(m['quads']), 'buffer lengths')
    require(all(0<=i<len(v) for i in ix), 'index outside vertex buffer')
    for q in m['quads']:
        n=q['first_vertex']; require(ix[n//4*6:n//4*6+6]==[n,n+1,n+2,n,n+2,n+3], 'quad winding')
        require(q['material']==[(q['uv_raw'][1]>>12)&15,(q['color']>>8)&127,(q['uv_raw'][0]>>12)&15], 'material association')
        for i in range(4):
            require(v[n+i][3:5]==[q['uv_raw'][2*i]&4095,q['uv_raw'][2*i+1]&4095], 'UV association')
            require(all(-8388608<=x<=8388607 for x in v[n+i][:3]), 'coordinate range')
    return len(v),len(ix)

def convert(a):
    rom=ROM(a.roms); out=a.out
    repo=pathlib.Path(__file__).resolve().parents[3]
    destination=out.resolve()
    if destination.is_relative_to(repo):
        require(destination.is_relative_to(repo/'raverace/native/generated'),
                'repository outputs must live under ignored raverace/native/generated/')
    require(not out.exists(), 'output must be a new directory (avoid stale assets)')
    require(69<=a.table_end<=len(rom.words), 'invalid object table boundary')
    models=[]; excluded=[]
    for code in range(69,a.table_end):
        if rom.word(code)<0: continue
        try:
            m=rom.model(code); validate_model(m); models.append(m)
        except ValueError as e:
            excluded.append(dict(code=code,reason=str(e)))
    require(models, 'no models decoded')
    out.mkdir(parents=True)
    files={}
    def emit(name,data):
        (out/name).write_bytes(data);files[name]=dict(bytes=len(data),sha256=digest(data))
    materials=set(); rectangles=set(); representative=set()
    for m in models:
        emit(f'model-{m["code"]}.json',encoded(m))
        emit(f'model-{m["code"]}.vertices',np.asarray(m['vertices'],dtype='<i4').reshape(-1,6).tobytes())
        emit(f'model-{m["code"]}.indices',np.asarray(m['indices'],dtype='<u4').tobytes())
        for q in m['quads']:
            materials.add(tuple(q['material']))
            u=q['uv_raw'][::2];v=q['uv_raw'][1::2]
            uu=[x&4095 for x in u];vv=[x&4095 for x in v]
            recipe=tuple(q['material'])+(min(uu),min(vv),max(uu)-min(uu)+1,max(vv)-min(vv)+1)
            rectangles.add(recipe)
            if m['code'] in a.bake_models: representative.add(recipe)
    palette=None
    if a.palette:
        b=a.palette.read_bytes();require(len(b)==128*256*3,'palette must be 128x256 RGB bytes')
        palette=np.frombuffer(b,np.uint8).reshape(128,256,3)
        emit('palette.rgb',b)
    # Bake a bounded representative selection; inventory reports whole-set cost.
    selected=sorted(representative) if a.bake_models else sorted(rectangles,key=lambda r:(r[5]*r[6],r))[:a.bake_count]
    require(len(selected)<=256,'representative set exceeds 256-material bound')
    for i,r in enumerate(selected):
        bank,pal,cmode,u,v,w,h=r;pen=rom.pens(bank,cmode,u,v,w,h)
        emit(f'material-{i}.pens',pen.tobytes())
        if palette is not None:
            rgba=np.empty((h,w,4),np.uint8);rgba[:,:,:3]=palette[pal][pen];rgba[:,:,3]=255
            emit(f'material-{i}.rgba',rgba.tobytes())
    emit('materials.json',encoded(dict(keys=sorted(materials),rectangles=sorted(rectangles),baked=selected,opaque=True)))
    stats=dict(models=len(models),quads=sum(len(m['quads']) for m in models),vertices=sum(len(m['vertices']) for m in models),
        material_keys=len(materials),unique_rectangles=len(rectangles),
        full_resolution_rgba_bytes=sum(r[5]*r[6]*4 for r in rectangles),
        referenced_tiles=len(np.unique(rom.tiles)),valid_referenced_tiles=len(np.unique(rom.tiles[rom.tiles<65536])),
        geometry_buffer_bytes=sum(f['bytes'] for n,f in files.items() if n.endswith(('.vertices','.indices'))))
    manifest=dict(format='rr-native-assets',version=VERSION,sources=rom.sources,table_end=a.table_end,
                  palette_sha256=digest(a.palette.read_bytes()) if a.palette else None,
                  files=files,stats=stats,excluded=excluded)
    (out/'manifest.json').write_bytes(encoded(manifest));print(json.dumps(stats,sort_keys=True));print('excluded',len(excluded))

def inspect(root):
    m=json.loads((root/'manifest.json').read_bytes())
    require(m['format']=='rr-native-assets' and m['version']==VERSION,'unsupported asset format/version')
    for name,info in m['files'].items():
        require(pathlib.PurePosixPath(name).name==name,'unsafe asset path')
        b=(root/name).read_bytes();require(len(b)==info['bytes'] and digest(b)==info['sha256'],f'checksum/size: {name}')
        if name.startswith('model-') and name.endswith('.json'):
            model=json.loads(b);validate_model(model)
            stem=name[:-5]
            require((root/(stem+'.vertices')).read_bytes()==np.asarray(model['vertices'],dtype='<i4').reshape(-1,6).tobytes(),'vertex buffer disagreement')
            require((root/(stem+'.indices')).read_bytes()==np.asarray(model['indices'],dtype='<u4').tobytes(),'index buffer disagreement')
    print(json.dumps(m['stats'],sort_keys=True));return m

def main():
    p=argparse.ArgumentParser();s=p.add_subparsers(dest='action',required=True)
    c=s.add_parser('convert');c.add_argument('roms',type=pathlib.Path);c.add_argument('out',type=pathlib.Path)
    c.add_argument('--table-end',type=lambda v:int(v,0),default=2490)
    c.add_argument('--palette',type=pathlib.Path);c.add_argument('--bake-count',type=int,default=32)
    c.add_argument('--bake-models',type=lambda v:[int(x,0) for x in v.split(',')],default=[])
    i=s.add_parser('inspect');i.add_argument('pack',type=pathlib.Path)
    a=p.parse_args()
    try:
        if a.action=='convert':
            require(0<=a.bake_count<=256,'bake-count must be 0..256');convert(a)
        else:inspect(a.pack)
    except (ValueError,OSError,KeyError) as e:
        print(f'asset error: {e}',file=sys.stderr);return 2
    return 0
if __name__=='__main__': sys.exit(main())
