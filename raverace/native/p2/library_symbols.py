#!/usr/bin/env python3
"""Use toolchain private symbols only after exact executable-section equality."""
import pathlib,struct,hashlib,json
root=pathlib.Path(__file__).resolve().parents[3];base=root/'raverace/native/generated/p2/symfs/lib';tool=pathlib.Path('/home/bwebb/dev/knulli-scarab/output/h700/host/aarch64-buildroot-linux-gnu/sysroot/lib');proof=[]
def text(path):
 data=path.read_bytes();h=struct.unpack_from('<16sHHIQQQIHHHHHH',data);off,entsize,num,strings=h[6],h[11],h[12],h[13];ss=[struct.unpack_from('<IIQQQQIIQQ',data,off+i*entsize) for i in range(num)];tab=ss[strings];names=data[tab[4]:tab[4]+tab[5]]
 return {names[s[0]:].split(b'\0',1)[0].decode():(s[3],data[s[4]:s[4]+s[5]]) for s in ss if s[2]&4 and s[1]==1}
for name in ['libc.so.6','libm.so.6']:
 actual=base/name;reference=tool/name
 target=base/('target-'+name)
 if actual.is_symlink():actual=target
 assert text(actual)==text(reference),name
 proof.append(dict(library=name,device_sha256=hashlib.sha256(actual.read_bytes()).hexdigest(),toolchain_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),all_executable_sections_exact=True,executable_sections={k:dict(address=addr,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()) for k,(addr,data) in text(actual).items()}))
 if not (base/name).is_symlink():actual.rename(target);(base/name).symlink_to(reference)
(root/'raverace/native/generated/p2/library-symbol-proof.json').write_text(json.dumps(proof,indent=2)+'\n');print('Exact device/toolchain executable sections verified; private library symbols usable')
