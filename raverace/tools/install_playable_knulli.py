#!/usr/bin/env python3
"""Apply an explicitly inventoried, reversible deployment plan on Knulli.

Default is a dry run. No ROM/save files are deleted, and no broad file glob is
used for mutation. Run only after physical/regression/performance validation.
"""
import argparse
import copy
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('plan', type=pathlib.Path)
parser.add_argument('binary', type=pathlib.Path)
parser.add_argument('launcher', type=pathlib.Path)
parser.add_argument('backup', type=pathlib.Path)
parser.add_argument('--apply', action='store_true')
args = parser.parse_args()
ports = pathlib.Path('/userdata/roms/ports')
app = ports/'rave-gles2'
primary = app/'rr_gles2'
menu = ports/'gamelist.xml'
final_launcher = ports/'Rave Racer.sh'
entries = json.loads(args.plan.read_text())

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024*1024), b''): h.update(block)
    return h.hexdigest()
def protected_source(path):
    if path.parent == app:
        return path.name in ('extracted', 'rr_gles2')
    if path.parent == ports:
        return not (path.name.startswith('RaveRacer-') and path.suffix=='.sh' or
                    path.name in ('Rave Racer - Candidate.sh','rr-ordered-fixed.sh','rave-gles2-cache-test'))
    return not (path.parent == ports/'rr-headless' and
                path.name in ('rr_headless','state-arm64.json','headless.log'))

assert args.backup.parent == pathlib.Path('/userdata/system/rave-racer-backups')
assert not args.backup.exists(), 'Backup must be a new directory'
assert not final_launcher.exists(), 'Do not overwrite an unclassified primary launcher'
assert len({e['source'] for e in entries}) == len(entries)
for entry in entries:
    source = pathlib.Path(entry['source']); relative = pathlib.Path(entry['destination'])
    assert entry['type'] in ('f','d')
    assert '..' not in source.parts
    assert ports in source.parents and not source.is_symlink(), source
    assert source.exists() and not protected_source(source), source
    if source==ports/'rave-gles2-cache-test': assert not any(source.iterdir()), 'Re-inventory nonempty cache-test directory'
    assert source.is_file() if entry['type']=='f' else source.is_dir(), source
    assert not relative.is_absolute() and '..' not in relative.parts, relative
    assert relative.parts[0] in ('archive','known-good'), relative
    assert 'extracted' not in source.parts and 'bios' not in source.parts and 'saves' not in source.parts
assert len({e['destination'] for e in entries}) == len(entries)
assert digest(primary)=='79f7abe165562b0dbf6e653a059fb0e3300cc936965ed59b45a9d6ae87ab242f'
assert digest(app/'rr-thread-final-arm64')=='2eb5e2e008a8d9c4ad3ef8d06c96fef97e81250ac1c55e529271a2999e555323'
with args.binary.open('rb') as f: header=f.read(20)
assert header[:6]==b'\x7fELF\x02\x01' and int.from_bytes(header[18:20],'little')==183
assert args.launcher.is_file()
print(f'{len(entries)} exact archive moves; preserve ROMs and copy known-good primary; deploy {args.binary}')
for entry in entries: print(entry['source'], '->', args.backup/entry['destination'], entry['role'])
if not args.apply: raise SystemExit(0)
assert subprocess.run(['pidof','emulationstation'], stdout=subprocess.DEVNULL).returncode != 0, 'Stop frontend before applying'
assert subprocess.run(['pidof','rr-playable-candidate-arm64','rr_gles2'], stdout=subprocess.DEVNULL).returncode != 0
args.backup.mkdir(parents=True)
(args.backup/'known-good').mkdir()
shutil.copy2(primary,args.backup/'known-good/rr_gles2')
assert digest(primary)==digest(args.backup/'known-good/rr_gles2')
shutil.copy2(args.plan,args.backup/'archive-plan.json')
roms=[app/'extracted',ports/'rr-headless/extracted']
rom_hashes={str(p):digest(p) for d in roms for p in d.iterdir() if p.is_file()}
(args.backup/'roms-preserved.json').write_text(json.dumps(rom_hashes,indent=2)+'\n')
if menu.exists(): shutil.copy2(menu,args.backup/'gamelist.xml.before')
file_hashes={e['source']:digest(pathlib.Path(e['source'])) for e in entries if e['type']=='f'}
(args.backup/'file-hashes-before.json').write_text(json.dumps(file_hashes,indent=2)+'\n')
move_log=[]
for entry in entries:
    source=pathlib.Path(entry['source']); target=args.backup/entry['destination']
    target.parent.mkdir(parents=True,exist_ok=True)
    assert not target.exists(), target
    # Same userdata filesystem, verified before deployment: rename, not delete.
    source.rename(target)
    move_log.append(entry)
    (args.backup/'moves-completed.json').write_text(json.dumps(move_log,indent=2)+'\n')
    if entry['type']=='f': assert digest(target)==file_hashes[str(source)], target

# Keep metadata for unrelated ports exactly as parsed; consolidate only the
# launchers in this plan. Retain play history and favourites from Rave entries.
if menu.exists():
    tree=ET.parse(menu); root=tree.getroot()
    removed={os.path.relpath(e['source'],ports) for e in entries if e['type']=='f'}
    selected=[g for g in root.findall('game') if (g.findtext('path') or '').removeprefix('./') in removed]
    unrelated=[ET.tostring(g) for g in root if g not in selected]
    if selected:
        new=copy.deepcopy(max(selected,key=lambda g:g.findtext('lastplayed') or ''))
        for tag,value in (('path','./Rave Racer.sh'),('name','Rave Racer')):
            node=new.find(tag)
            if node is None: node=ET.SubElement(new,tag)
            node.text=value
        for tag in ('playcount','gametime'):
            try: value=sum(int(g.findtext(tag) or '0') for g in selected)
            except ValueError: continue
            node=new.find(tag)
            if node is None: node=ET.SubElement(new,tag)
            node.text=str(value)
        if any(g.findtext('favorite')=='true' for g in selected):
            node=new.find('favorite')
            if node is None: node=ET.SubElement(new,'favorite')
            node.text='true'
        for g in selected: root.remove(g)
    else:
        new=ET.Element('game');ET.SubElement(new,'path').text='./Rave Racer.sh';ET.SubElement(new,'name').text='Rave Racer'
    root.append(new)
    assert [ET.tostring(g) for g in root if g is not new]==unrelated
    temp=ports/'gamelist.xml.rave-next'
    assert not temp.exists()
    tree.write(temp,encoding='utf-8',xml_declaration=True);temp.replace(menu)
    shutil.copy2(menu,args.backup/'gamelist.xml.after')

staged=app/'rr_gles2.playable-next'
assert not staged.exists()
shutil.copyfile(args.binary,staged);staged.chmod(0o755)
assert digest(staged)==digest(args.binary)
staged.replace(primary)
shutil.copyfile(args.launcher,final_launcher);final_launcher.chmod(0o755)
for path,value in rom_hashes.items(): assert digest(pathlib.Path(path))==value, path
manifest={'primary':str(primary),'sha256':digest(primary),'launcher':str(final_launcher),
          'backup':str(args.backup),'archived_entries':len(entries),'roms_unchanged':True}
(args.backup/'deployment.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
print('Restart EmulationStation and verify the single Rave Racer entry.')
