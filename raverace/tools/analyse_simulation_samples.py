#!/usr/bin/env python3
"""Symbolise native PC samples using matching local ELF + recorded process maps."""
import bisect,collections,csv,pathlib,subprocess,sys
binary,sample_file,nm,addr2line=sys.argv[1:5]
end=int(sys.argv[5]) if len(sys.argv)>5 else 2**31-1
symbols=[]
for line in subprocess.check_output([nm,'-n','--defined-only',binary],text=True).splitlines():
 parts=line.split()
 if len(parts)==3 and parts[1] in 'tT':symbols.append((int(parts[0],16),parts[2]))
starts=[s[0] for s in symbols]
maps=[]
for line in pathlib.Path(sample_file+'.maps').read_text().splitlines():
 parts=line.split()
 if len(parts)>=6:
  lo,hi=(int(v,16) for v in parts[0].split('-'))
  maps.append((lo,hi,int(parts[2],16),parts[-1]))
is_pie='DYN' in next(l for l in subprocess.check_output(['readelf','-h',binary],text=True).splitlines() if 'Type:' in l)
counts=collections.Counter();pcs=collections.Counter();stages=collections.Counter();total=0
for row in csv.DictReader(open(sample_file),delimiter='\t'):
 if int(row['frame'])>end:continue
 pc=int(row['native_pc'],16);stage=row['stage'];total+=1;stages[stage]+=1
 mapping=next((m for m in maps if m[0]<=pc<m[1]),None)
 if mapping and 'rr_gles2' in pathlib.Path(mapping[3]).name:
  address=pc-(mapping[0]-mapping[2]) if is_pie else pc
  n=bisect.bisect_right(starts,address)-1
  name=symbols[n][1] if n>=0 else '[ELF unknown]'
  pcs[(stage,name,address)]+=1
 else:name='[external '+(pathlib.Path(mapping[3]).name if mapping else 'unmapped')+']'
 counts[(stage,name)]+=1
print('CPU-triggered samples',total,'(no call stacks; caller-inclusive costs unavailable)')
for stage,n in stages.most_common():print('STAGE',stage,n,f'{n/total*100:.2f}%')
for (stage,name),n in counts.most_common(35):print('FUNCTION',stage,name,n,f'{n/total*100:.2f}%')
# Give representative instruction PCs for subsequent disassembly.
for (stage,name,address),n in pcs.most_common(15):
 loc=subprocess.check_output([addr2line,'-f','-e',binary,hex(address)],text=True).strip().replace('\n',' ')
 print('PC',stage,name,hex(address),n,loc)
with open(sample_file+'.functions.tsv','w') as f:
 f.write('stage\tfunction\tsamples\tpercent\n')
 for (stage,name),n in counts.most_common():f.write(f'{stage}\t{name}\t{n}\t{n/total*100:.4f}\n')
