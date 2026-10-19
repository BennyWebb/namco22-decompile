#!/usr/bin/env python3
"""Explicit local app installation, with one-step rollback; never run by tests."""
from pathlib import Path
import argparse,shutil,subprocess,time
p=argparse.ArgumentParser();p.add_argument('package',type=Path);p.add_argument('--install',action='store_true');a=p.parse_args()
if not a.install:p.error('explicit --install required')
assert subprocess.run(['pidof','rr_gles3','rr_gles2'],stdout=subprocess.DEVNULL).returncode!=0,'exit game first'
app=Path('/userdata/system/rave-racer-gles3');backup=app.with_name(app.name+'-rollback-'+str(int(time.time())))
previous=app.exists()
if previous:shutil.copytree(app,backup)
shutil.copytree(a.package,app,dirs_exist_ok=True)
ports=Path('/userdata/roms/ports')
for n,flags in [('Rave Racer (GLES3).sh',''),('Rave Racer (GLES3, record).sh','--record')]:
 q=ports/n;q.write_text('#!/bin/sh\nexec python3 '+str(app/'session.py')+' '+flags+'\n');q.chmod(0o755)
restore=app/'restore.sh'
action=('cp -a '+str(backup)+'/./ '+str(app)+'/\necho "Previous GLES3 app restored"\n') if previous else ('rm -f "'+str(ports/'Rave Racer (GLES3).sh')+'" "'+str(ports/'Rave Racer (GLES3, record).sh')+'"\necho "First installation disabled; app, saves and recordings retained"\n')
restore.write_text('#!/bin/sh\nset -eu\nif pidof rr_gles3 >/dev/null;then echo "Exit game first";exit 4;fi\n'+action);restore.chmod(0o755)
print('Installed',app,'rollback:',restore)
