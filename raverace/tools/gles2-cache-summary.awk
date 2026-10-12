# POSIX awk: one application's log followed by its one-second memory samples.
function field(line, key,    n,a,b,i) {
    n=split(line,a," "); for(i=1;i<=n;i++) {split(a[i],b,"=");if(b[1]==key)return b[2]}
    return "NA"
}
function mib(k) { return k=="NA" || k<0 ? "NA" : sprintf("%.1f",k/1024) }
FILENAME==ARGV[1] {
    if(/^\[PROFILE\] total /) profile=$0
    if(/^\[PROFILE-CACHE\] total /) cache=$0
    if(/^\[STAGES-RATE\] total /) rate=$0
    if(/^\[TEX\] decoded=/) texture=$0
    if(/^\[MEMORY\]/) memory=$0
    if(/GL error 0x|shader compilation failed|program link failed|upload\/allocation failure|geometry allocation failed/) render_error=1
    next
}
FILENAME==ARGV[2] && $1 ~ /^[0-9]+$/ {
    if($2!="NA" && $2+0>rss)rss=$2+0
    if($3!="NA" && $3+0>rss)rss=$3+0
    if($4!="NA" && (!have_mem || $4+0<mem_min)){mem_min=$4+0;have_mem=1}
    if($5!="NA" && (!have_cma || $5+0<cma_min)){cma_min=$5+0;have_cma=1}
}
END {
    steps=field(rate,"steps");wall=field(rate,"wall_s");fps="NA";avg="NA"
    valid=profile!="" && cache!="" && rate!="" && steps==expected_samples &&
        field(profile,"samples")==expected_samples && field(rate,"skipped")==0 &&
        field(profile,"frames")==expected_range && field(rate,"timing")=="coupled" &&
        field(profile,"mode")=="textured" && field(profile,"vbo")=="frame" &&
        field(texture,"budget")==budget*1048576 && field(texture,"peak")<=budget*1048576
    if(valid && wall+0>0){avg=sprintf("%.3f",1000*wall/steps);fps=sprintf("%.2f",steps/wall)}
    peak_rss=field(memory,"RSS_peak_kib");if(peak_rss!="NA" && peak_rss+0>rss)rss=peak_rss+0
    end_rss=field(memory,"RSS_kib");if(end_rss!="NA" && end_rss+0>rss)rss=end_rss+0
    end_avail=field(memory,"MemAvailable_kib")
    if(end_avail!="NA" && end_avail+0>=0 && (!have_mem || end_avail+0<mem_min)){mem_min=end_avail+0;have_mem=1}
    if(exit_code!=0)status="EXIT_" exit_code
    else if(!valid)status="PROFILE_FAIL"
    else if(render_error)status="RENDER_ERROR"
    else if(state_status!="SAME")status="STATE_" state_status
    else status="OK"
    gpu=field(texture,"peak");gpu_mib=gpu=="NA"?"NA":sprintf("%.1f",gpu/1048576)
    printf "%d\t%s\t%s",budget,avg,fps
    names="simulation scene lookup_decode uploads submission swap vertex_upload render_other"
    n=split(names,keys," ");for(i=1;i<=n;i++)printf "\t%s",field(profile,keys[i])
    printf "\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
        field(cache,"hits"),field(cache,"misses"),field(cache,"evictions"),field(cache,"texture_uploads"),gpu_mib,
        field(cache,"GPU_frame_end_peak"),mib(rss?rss:"NA"),mib(have_mem?mem_min:"NA"),mib(have_cma?cma_min:"NA"),
        state_hash,state_status,status
    if(status!="OK")exit 1
}
