#!/usr/bin/env python3
"""Run the existing ownership stress against the new generated allocator capacity."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3];b=root/'raverace/native/generated/g1c';s=(b/'b2-validation/host/live.c').read_text();(b/'dynamic-current.h').write_text(s[s.index('/* CPU allocator owns'):s.index('static G1BSlot*g1b_slot;')]);t=(root/'raverace/native/g1b/test_allocator.c').read_text().replace('#include "dynamic.h"','#include "dynamic-current.h"').replace('*1024','*G1B_SIDE').replace('1021,1','G1B_SIDE-3,1');(b/'test-allocator-current.c').write_text(t);subprocess.run(['cc','-O2','-std=gnu11',str(b/'test-allocator-current.c'),'-o',str(b/'test_allocator')],check=True);subprocess.run([str(b/'test_allocator')],check=True)
