#!/usr/bin/env python3
"""Show recording length and optional X/F3 race-section markers."""
import pathlib
import sys
path=pathlib.Path(sys.argv[1])
end=int(pathlib.Path(str(path)+'.end').read_text())
events=0
timing=next(l.split()[1] for l in pathlib.Path(str(path)+'.meta').read_text().splitlines() if l.startswith('TIMING '))
numerator,denominator=map(int,timing.split('/'))
seconds=denominator/numerator
print(f'Completed ticks: {end}; emulated duration: {end*seconds:.2f}s')
for line in path.read_text().splitlines():
    if line.startswith('F '):events+=1
    if line.startswith('# MARK '):
        _,_,frame,index=line.split()
        print(f'Marker {index}: tick {frame}, emulated time {int(frame)*seconds:.2f}s')
print(f'Input changes: {events}; input file: {path.stat().st_size} bytes')
print('Use your race-start/finish markers to exclude boot, menus and countdown. Markers are annotations, not game state.')
