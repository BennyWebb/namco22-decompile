#!/usr/bin/env python3
"""Create durable full-window side-by-side review frames; no timing claims."""
import argparse,pathlib,subprocess,concurrent.futures,shutil,json
p=argparse.ArgumentParser();p.add_argument('artifacts',type=pathlib.Path);a=p.parse_args();base=a.artifacts.resolve();out=base/'review';(out/'frames').mkdir(parents=True,exist_ok=True)
for tick in [3001,3002]:
 source=base/'reference-gaps'/f'hud-{tick}.reference.rgba';dest=base/'reference-sequence'/source.name
 if not dest.exists():shutil.copyfile(source,dest)
assert len(list((base/'reference-sequence').glob('*.rgba')))==3840
assert len(list((base/'native-sequence').glob('*.rgba')))==3840

def make(tick):
 ref=base/'reference-sequence'/f'hud-{tick}.reference.rgba';native=base/'native-sequence'/f'tick-{tick}.rgba';target=out/'frames'/f'{tick}.jpg'
 subprocess.run(['magick','(','-size','640x480','-depth','8','rgba:'+str(ref),'-flip','-alpha','off',')','(','-size','640x480','-depth','8','rgba:'+str(native),'-flip','-alpha','off',')','+append','-quality','92',str(target)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:list(executor.map(make,range(1800,5640)))
(out/'index.html').write_text('''<!doctype html><meta charset="utf-8"><title>Milestone 3 race comparisons</title><style>body{font:16px system-ui;background:#17191c;color:#eee;max-width:1320px;margin:24px auto}img{width:100%;height:auto}input{width:65%}button,select{font:inherit}a{color:#a9d9ff}</style><h1>Milestone 3 race comparison</h1><p>Original renderer on the left · native renderer on the right. All 3,840 ticks are available. JPEGs are review previews; lossless raw captures are preserved alongside the report.</p><p><button id="play">Play</button> <button id="prev">Previous</button> <button id="next">Next</button> Tick <span id="tick"></span> <select id="step"><option value="1">Every tick</option><option value="4" selected>Every fourth tick</option></select></p><input id="slider" type="range" min="1800" max="5639" value="1800"><img id="frame" alt="Original left, native right"><p>Playback speed depends on image loading in your browser and is not a renderer performance measurement.</p><script>const image=document.querySelector('#frame'),slider=document.querySelector('#slider'),label=document.querySelector('#tick');let playing=false;function show(t){slider.value=Math.max(1800,Math.min(5639,t));label.textContent=slider.value;image.src='frames/'+slider.value+'.jpg'}slider.oninput=()=>{playing=false;show(+slider.value)};document.querySelector('#prev').onclick=()=>{playing=false;show(+slider.value-1)};document.querySelector('#next').onclick=()=>{playing=false;show(+slider.value+1)};document.querySelector('#play').onclick=()=>{playing=!playing;if(playing)show(+slider.value)};image.onload=()=>{if(playing)setTimeout(()=>{if(+slider.value>=5639){playing=false;return}show(+slider.value+(+document.querySelector('#step').value))},1000/59.904*(+document.querySelector('#step').value))};show(1800);</script>''')
print(out/'index.html')
