"""Bounded original-source race packet reader (RSC3 version 1)."""
import pathlib,struct,hashlib
import numpy as np
MAGIC=0x33435352
STATE_BYTES=0x18000+0x8000+0x8000+0x1e000+0x2000+16
MAX_QUADS=32768
MAX_DIRECT=4096

def read(path):
 b=pathlib.Path(path).read_bytes()
 if len(b)<32:raise ValueError('truncated packet')
 magic,version,tick,walk,n,d,stride,state=struct.unpack_from('<8I',b)
 if magic!=MAGIC or version!=1:raise ValueError('unknown scene protocol')
 if not 1800<=tick<=5639 or walk>1 or n>MAX_QUADS or d>MAX_DIRECT or stride!=352 or state!=STATE_BYTES:raise ValueError('invalid packet bounds')
 off=32+n*352
 if len(b)!=off+d*56+state:raise ValueError('packet size mismatch')
 q=np.frombuffer(b,'<i4',n*88,32).reshape(n,88)
 direct=np.frombuffer(b,'<u2',d*28,off).reshape(d,28);off+=d*56
 arrays={}
 for name,size in [('palette',0x18000),('mixer',0x8000),('czram',0x8000),('cgram',0x1e000),('text',0x2000),('attributes',16)]:arrays[name]=b[off:off+size];off+=size
 return dict(tick=tick,walk=walk,quads=q,direct=direct,**arrays,sha256=hashlib.sha256(b).hexdigest())

def recipes(frame):
 q=frame['quads'];local=q[:,36:60].reshape(-1,4,6);u=local[:,:,3];v=local[:,:,4]
 if len(q):
  r=np.column_stack([q[:,5],(q[:,3]>>8)&127,q[:,6],u.min(1),v.min(1),u.max(1)-u.min(1)+1,v.max(1)-v.min(1)+1])
 else:r=np.empty((0,7),dtype=np.int64)
 d=frame['direct'];out=[]
 for words in d:
  u=words[[4,10,16,22]]&4095;v=words[[5,11,17,23]]&4095
  out.append([int(words[5]>>12),int(words[2]>>8)&127,int(words[4]>>12),int(u.min()),int(v.min()),int(u.max()-u.min()+1),int(v.max()-v.min()+1)])
 return [tuple(map(int,x)) for x in r]+[tuple(x) for x in out]
