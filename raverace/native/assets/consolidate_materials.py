#!/usr/bin/env python3
"""Offline source-UV material consolidation; no incumbent bake resource relocation."""
import argparse,hashlib,importlib.util,json,math,pathlib,struct
import numpy as np
SPEC=importlib.util.spec_from_file_location('asset_tool',pathlib.Path(__file__).with_name('asset_tool.py'))
asset=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(asset)
def sha(b):return hashlib.sha256(b).hexdigest()
def canonical(x):return (json.dumps(x,sort_keys=True,separators=(',',':'))+'\n').encode()
def contains(a,b):return a[0]<=b[0] and a[1]<=b[1] and a[0]+a[2]>=b[0]+b[2] and a[1]+a[3]>=b[1]+b[3]
def pack(items,extent):
    pages=[];placements={}
    for key,w,h in sorted(items,key=lambda x:(-max(x[1:]),-x[1]*x[2],x[0])):
        if w>extent or h>extent:raise ValueError(f'material {key} exceeds page extent: {w}x{h}')
        fits=[]
        for page,free in enumerate(pages):
            for r in free:
                if w<=r[2] and h<=r[3]:fits.append(((min(r[2]-w,r[3]-h),max(r[2]-w,r[3]-h),page,r[1],r[0]),page,r))
        if not fits:
            pages.append([(0,0,extent,extent)]);page=len(pages)-1;r=pages[page][0]
        else:_,page,r=min(fits)
        x,y=r[:2];used=(x,y,w,h);new=[]
        for fx,fy,fw,fh in pages[page]:
            if x>=fx+fw or x+w<=fx or y>=fy+fh or y+h<=fy:new.append((fx,fy,fw,fh));continue
            if x>fx:new.append((fx,fy,x-fx,fh))
            if x+w<fx+fw:new.append((x+w,fy,fx+fw-x-w,fh))
            if y>fy:new.append((fx,fy,fw,y-fy))
            if y+h<fy+fh:new.append((fx,y+h,fw,fy+fh-y-h))
        unique=sorted(set(new));pages[page]=[r for i,r in enumerate(unique) if not any(i!=j and contains(other,r) for j,other in enumerate(unique))]
        placements[key]=(page,x,y,w,h)
    dims=[]
    for page in range(len(pages)):
        rs=[r for r in placements.values() if r[0]==page]
        # 4-texel alignment; NPOT clamp/nearest baseline, mipmaps remain optional.
        dims.append((math.ceil(max(r[1]+r[3] for r in rs)/4)*4,math.ceil(max(r[2]+r[4] for r in rs)/4)*4))
    return placements,dims

def direct_records(path):
    if path is None:return []
    b=path.read_bytes()
    if len(b)<4:raise ValueError('truncated direct snapshot')
    count=struct.unpack_from('<I',b)[0]
    if count>4096 or len(b)!=4+count*56:raise ValueError('invalid direct snapshot count')
    result=[]
    for index,words in enumerate(struct.iter_unpack('<28H',b[4:])):
        bank=words[5]>>12;group=(words[2]>>8)&127;cmode=words[4]>>12
        uv=[[words[4+6*i]&4095,words[5+6*i]&4095] for i in range(4)]
        u=[x[0] for x in uv];v=[x[1] for x in uv]
        result.append(dict(index=index,raw=list(words),recipe=[bank,group,cmode,min(u),min(v),max(u)-min(u)+1,max(v)-min(v)+1],zsort=((words[1]&4095)<<12)|(words[0]&4095)))
    return result

def compile_pack(a):
    repo=pathlib.Path(__file__).resolve().parents[3];out=a.out.resolve()
    if out.exists():raise ValueError('output must be a new directory')
    if out.is_relative_to(repo) and not out.is_relative_to(repo/'raverace/native/generated'):raise ValueError('repository outputs must use ignored native/generated')
    rom=asset.ROM(a.roms);manifest=asset.inspect(a.pack)
    if rom.sources!=manifest['sources']:raise ValueError('source ROM provenance differs from native asset pack')
    palbytes=a.palette.read_bytes()
    if len(palbytes)!=128*256*3:raise ValueError('palette must be 128x256 RGB8')
    pal=np.frombuffer(palbytes,np.uint8).reshape(128,256,3)
    records=[json.loads(s) for s in (a.instances/'quads.jsonl').read_text().splitlines()];models={};recipe_records={};links=[]
    for r in records:
        code=r['code']
        if code==5:raise ValueError('runtime point RAM requires a separately versioned recipe')
        if code not in models:
            m=json.loads((a.pack/f'model-{code}.json').read_bytes());asset.validate_model(m)
            lookup={q['packet_address']+1+(3 if rom.word(q['packet_address'])==23 else 4):q for q in m['quads']};models[code]=(m,lookup)
        m,lookup=models[code];q=lookup[r['source_data']];u=[v&4095 for v in q['uv_raw'][::2]];v=[v&4095 for v in q['uv_raw'][1::2]]
        recipe=tuple(q['material'])+(min(u),min(v),max(u)-min(u)+1,max(v)-min(v)+1)
        key=sha(canonical(recipe));recipe_records[key]=recipe
        links.append(dict(code=code,source_data=r['source_data'],packet_address=q['packet_address'],first_vertex=q['first_vertex'],material=key))
    directs=direct_records(a.direct)
    for r in directs:
        recipe=tuple(r['recipe']);key=sha(canonical(recipe));recipe_records[key]=recipe;links.append(dict(kind='direct',direct_index=r['index'],material=key))
    contents={};materials={};g=a.gutter
    for key,r in sorted(recipe_records.items()):
        bank,group,cmode,u,v,w,h=r
        pens=rom.pens(bank,cmode,(u-g)&4095,(v-g)&4095,w+2*g,h+2*g)
        rgba=np.empty((h+2*g,w+2*g,4),np.uint8);rgba[:,:,:3]=pal[group][pens];rgba[:,:,3]=255
        blob=rgba.tobytes();content=sha(struct.pack('<II',w+2*g,h+2*g)+blob)
        contents.setdefault(content,rgba);materials[key]=dict(recipe=r,content=content,source_uv_bounds=[u,v,u+w-1,v+h-1],palette_group=group)
    places,dims=pack([(k,x.shape[1],x.shape[0]) for k,x in contents.items()],a.extent)
    pages=[np.zeros((h,w,4),np.uint8) for w,h in dims]
    for k,image in contents.items():
        page,x,y,w,h=places[k];pages[page][y:y+h,x:x+w]=image
    for info in materials.values():
        page,x,y,w,h=places[info['content']];u,v,ww,hh=info['recipe'][3:];pw,ph=dims[page]
        info.update(page=page,rect=[x,y,w,h],gutter=g,native_uv_affine=[1/pw,1/ph,(x+g+.5-u)/pw,(y+g+.5-v)/ph])
    def mipbytes(w,h):
        total=0
        while True:
            total+=w*h*4
            if w==h==1:return total
            w=max(1,w//2);h=max(1,h//2)
    pagebytes=sum(w*h*4 for w,h in dims);mips=sum(mipbytes(w,h) for w,h in dims)
    geometry=sum((a.pack/f'model-{c}.{ext}').stat().st_size for c in models for ext in ['vertices','indices'])
    raw=(a.scene/'draws.bin').read_bytes();pos=0;deg=0;scissors=set();triangles=0;draw_count=0
    while pos<len(raw):
        if len(raw)-pos<24:raise ValueError('truncated captured draw header')
        header=struct.unpack_from('<6I',raw,pos);pos+=24;count=header[1]
        if count%3 or count>1000000 or len(raw)-pos<count*40:raise ValueError('invalid captured vertex count')
        verts=np.frombuffer(raw,dtype='<f4',count=count*10,offset=pos).reshape(-1,10);pos+=count*40;points=verts[:,:2].reshape(-1,3,2).astype(float)
        area=(points[:,1,0]-points[:,0,0])*(points[:,2,1]-points[:,0,1])-(points[:,2,0]-points[:,0,0])*(points[:,1,1]-points[:,0,1]);deg+=int(np.any(area==0));triangles+=count//3;draw_count+=1;scissors.add(tuple(header[2:]))
    # Bound for opaque page/scissor groups plus individually protected captured
    # degenerate draws. Per-degenerate page splits are an additional unknown.
    group_estimate=len(pages)*len(scissors)+deg+len(directs)
    report=dict(direct_quads=len(directs),source_quads=len(records),active_models=len(models),source_recipes=len(materials),unique_rgba_rectangles=len(contents),
        source_rgba_bytes=sum(r[5]*r[6]*4 for r in recipe_records.values()),gutter_content_bytes=sum(x.nbytes for x in contents.values()),
        physical_pages=[list(d) for d in dims],base_page_bytes=pagebytes,mip_chain_page_bytes=mips,geometry_buffer_bytes=geometry,
        reserved_native_scene_bytes=8*1024*1024,residency_base_bytes=pagebytes+geometry+8*1024*1024,residency_with_mips_bytes=mips+geometry+8*1024*1024,
        cpu_source_bytes=rom.words.nbytes+rom.cg.nbytes+rom.tiles.nbytes+rom.attr.nbytes+len(palbytes),cpu_material_staging_bytes=sum(x.nbytes for x in contents.values()),cpu_page_staging_bytes=pagebytes,
        source_triangles=triangles,original_draws=draw_count,protected_degenerate_draws=deg,scissor_regions=len(scissors),estimated_opaque_draws=group_estimate,
        packing_occupancy=sum(x.nbytes for x in contents.values())/pagebytes,
        caveat='Draw estimate excludes additional page splits in protected degenerate draws and native clip topology; those must be measured before the full backend gate.',
        baseline_filter='nearest, no mipmaps',optional_mip_policy='Independently filter each material footprint with per-level regenerated source-aware gutters; whole-page mip filtering is forbidden. Mip cost is reserved, not emitted/enabled.')
    out.mkdir(parents=True);files={}
    def emit(name,b):
        (out/name).write_bytes(b);files[name]=dict(bytes=len(b),sha256=sha(b))
    for i,page in enumerate(pages):emit(f'page-{i}.rgba',page.tobytes())
    emit('materials.json',canonical(dict(materials=materials,source_links=links,pages=dims)))
    emit('budget.json',canonical(report));emit('palette.rgb',palbytes)
    if directs:emit('direct.json',canonical(directs))
    result=dict(format='rr-native-material-pages',version=1,gutter=g,extent=a.extent,opaque=True,filter='nearest',mipmaps=False,
        direct_snapshot_sha256=sha(a.direct.read_bytes()) if a.direct else None,native_asset_manifest_sha256=sha((a.pack/'manifest.json').read_bytes()),source_snapshot_sha256=sha((a.instances/'quads.jsonl').read_bytes()),palette_sha256=sha(palbytes),rom_sources=rom.sources,files=files)
    (out/'manifest.json').write_bytes(canonical(result));print(json.dumps(report,indent=2))

def main():
    p=argparse.ArgumentParser();p.add_argument('roms',type=pathlib.Path);p.add_argument('pack',type=pathlib.Path);p.add_argument('instances',type=pathlib.Path);p.add_argument('scene',type=pathlib.Path);p.add_argument('palette',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--direct',type=pathlib.Path);p.add_argument('--extent',type=int,default=4096);p.add_argument('--gutter',type=int,default=2);a=p.parse_args()
    if a.extent not in (1024,2048,4096) or a.gutter not in (1,2,4):p.error('extent must be 1024/2048/4096; gutter must be 1/2/4')
    try:compile_pack(a)
    except (ValueError,OSError,KeyError) as e:p.exit(2,f'material compiler: {e}\n')
if __name__=='__main__':main()
