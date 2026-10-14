#!/usr/bin/env python3
"""Real ROM/pack/frozen resource validation; outputs contain statistics only."""
import importlib.util
import json
import pathlib
import sys
import tempfile
import numpy as np
spec=importlib.util.spec_from_file_location('asset_tool',pathlib.Path(__file__).parents[1]/'assets/asset_tool.py')
t=importlib.util.module_from_spec(spec);spec.loader.exec_module(t)
romdir,pack,scene,palette=map(pathlib.Path,sys.argv[1:5])
r=t.ROM(romdir);m=t.inspect(pack)
# Decode source again, and compare stored object-local coordinates and attributes.
for code in range(69,m['table_end']):
    if r.word(code)<0:continue
    actual=json.loads((pack/f'model-{code}.json').read_bytes())
    assert actual==r.model(code),code
# Validate decoder independently against the existing renderer's captured payloads.
pal=np.frombuffer(palette.read_bytes(),np.uint8).reshape(128,256,3)
texels=resources=0
for line in (scene/'resources.txt').read_text().splitlines():
    rid,*k=map(int,line.split());w,h=k[1:3]
    with (scene/f'texture-{rid}.rgba').open('rb') as f:
        f.readline();actual=np.frombuffer(f.read(),np.uint8).reshape(h,w,4)
    if w==h==1 and not any(k[3:]): expected=np.full((1,1,4),255,np.uint8)
    else:
        su=max(1,(k[5]+k[11]-1)//k[11]);sv=max(1,(k[6]+k[11]-1)//k[11])
        # Compiler is full resolution; sample the same source strides as incumbent bake.
        full=r.pens(k[7],k[10],k[3],k[4],(w-1)*su+1,(h-1)*sv+1)
        pen=full[::sv,::su]
        expected=np.empty((h,w,4),np.uint8);expected[:,:,:3]=pal[k[8]][pen];expected[:,:,3]=255
    assert np.array_equal(actual,expected),f'frozen resource {rid}'
    texels+=w*h;resources+=1
# Actual truncated ROM, invalid version, corrupted buffer and malformed packet gates.
for address in (-1,len(r.words)):
    try:r.word(address)
    except ValueError:pass
    else:raise AssertionError('accepted invalid address')
saved=r.words.copy();r.words[69]=len(r.words)
try:r.model(69)
except ValueError:pass
else:raise AssertionError('accepted invalid list pointer')
r.words=saved
with tempfile.TemporaryDirectory(prefix='rr-m1-short-rom-') as temp:
    root=pathlib.Path(temp)
    for name in r.sources:
        (root/name).symlink_to((romdir/name).resolve())
    first=root/'rv1potl0.5b';first.unlink();first.write_bytes(b'short')
    try:t.ROM(root)
    except ValueError:pass
    else:raise AssertionError('accepted truncated ROM')
listaddr=r.word(69);chunk=r.word(listaddr);saved_value=r.words[chunk+1]
try:
    r.words[chunk+1]=99
    try:r.model(69)
    except ValueError:pass
    else:raise AssertionError('accepted unknown packet')
finally:r.words[chunk+1]=saved_value
manifest=pack/'manifest.json';original=manifest.read_bytes();bad=json.loads(original);bad['version']=999
try:
    manifest.write_bytes(t.encoded(bad))
    try:t.inspect(pack)
    except ValueError:pass
    else:raise AssertionError('accepted unknown version')
finally:manifest.write_bytes(original)
file=pack/'model-69.indices';original=file.read_bytes()
try:
    file.write_bytes(original+b'bad')
    try:t.inspect(pack)
    except ValueError:pass
    else:raise AssertionError('accepted corrupt buffer')
finally:file.write_bytes(original)
# Validate all emitted material bytes against declared recipes and actual palette.
mat=json.loads((pack/'materials.json').read_bytes())
for i,(bank,group,cmode,u,v,w,h) in enumerate(mat['baked']):
    pen=r.pens(bank,cmode,u,v,w,h)
    assert (pack/f'material-{i}.pens').read_bytes()==pen.tobytes()
    rgba=np.frombuffer((pack/f'material-{i}.rgba').read_bytes(),np.uint8).reshape(h,w,4)
    assert np.array_equal(rgba[:,:,:3],pal[group][pen]) and np.all(rgba[:,:,3]==255)
print(json.dumps(dict(source_models=m['stats']['models'],frozen_resources=resources,
    frozen_texels=texels,differing_texels=0,baked_materials=len(mat['baked']),negative_gates='passed'),sort_keys=True))
