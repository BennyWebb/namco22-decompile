"""Bounded v3 scene wire reader shared by validators and review tools."""
import pathlib,struct,zlib
import numpy as np
RAW_MAX=524288
QUAD=np.dtype([('mesh','<i4'),('instance','<u2'),('material','<u2'),('depth','<f4'),('brightness','<u2',(4,)),('source_instance','<u2'),('order','<u2'),('flags','<u2'),('reserved','<u2'),('zsort','<u4')])
class Reader:
 def __init__(self,path):
  self.path=pathlib.Path(path);self.stream=self.path.open('rb');h=self.stream.read(16)
  if len(h)!=16 or struct.unpack('<4I',h)!=(0x33525352,3,3840,1):raise ValueError('unknown compact stream')
  b=self.stream.read(3841*8)
  if len(b)!=3841*8:raise ValueError('truncated offsets')
  self.offsets=struct.unpack('<3841Q',b)
  if self.offsets[0]!=16+len(b) or self.offsets[-1]!=self.path.stat().st_size or any(not 0<y-x<=RAW_MAX for x,y in zip(self.offsets,self.offsets[1:])):raise ValueError('invalid stream offsets')
 def read(self,tick):
  if not 1800<=tick<=5639:raise ValueError('tick outside captured window')
  i=tick-1800;self.stream.seek(self.offsets[i]);b=self.stream.read(self.offsets[i+1]-self.offsets[i]);decoder=zlib.decompressobj()
  try:raw=decoder.decompress(b,RAW_MAX+1)
  except zlib.error as e:raise ValueError('compressed checksum/format') from e
  if len(raw)>RAW_MAX or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:raise ValueError('invalid compressed packet')
  if len(raw)<60:raise ValueError('truncated header')
  h=struct.unpack_from('<12I3f',raw);magic,version,t,n,nv,ng,ni,nd,font,style,palette,reserved=h[:12]
  if magic!=0x33504e52 or version!=3 or t!=tick or n>4096 or nv>n or ng>4096 or ni>512 or nd>4096 or font>=16 or style>=4096 or reserved:raise ValueError('invalid frame bounds/version')
  expected=60+ni*80+n*32+((nv*2+3)&~3)+ng*32+nd*16+8192
  if len(raw)!=expected:raise ValueError('frame size mismatch')
  at=60;instances=np.frombuffer(raw,'<f4',ni*20,at).reshape(ni,20);at+=ni*80
  quads=np.frombuffer(raw,QUAD,n,at);at+=n*32;visible=np.frombuffer(raw,'<u2',nv,at);at+=(nv*2+3)&~3
  groups=np.frombuffer(raw,'<i4',ng*8,at).reshape(ng,8);at+=ng*32;dynamic=np.frombuffer(raw,'<f4',nd*4,at).reshape(nd,4);at+=nd*16
  return dict(raw=raw,compressed=b,header=h,instances=instances,quads=quads,visible=visible,groups=groups,dynamic=dynamic,text=raw[at:])
