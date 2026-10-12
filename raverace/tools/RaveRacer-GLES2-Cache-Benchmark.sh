#!/bin/sh
# Fixed scripted throughput experiment; neither interactive nor real-time paced.
set -u
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
app_dir=${RR_CACHE_APP_DIR:-$script_dir/rave-gles2}
app_dir=$(CDPATH= cd -- "$app_dir" && pwd) || exit 1
summary_awk=$script_dir/gles2-cache-summary.awk
[ -r "$summary_awk" ] || { printf 'Missing %s\n' "$summary_awk" >&2; exit 2; }
binary=${RR_CACHE_BINARY:-rr_gles2}
if [ "${RR_CACHE_BINARY+x}" != x ] && [ -x "$app_dir/rr_gles2-cache-bench" ]; then binary=rr_gles2-cache-bench;fi
case "$binary" in /*) ;; *) binary=$app_dir/$binary;; esac
[ -x "$binary" ] || { printf 'Missing executable: %s\n' "$binary" >&2; exit 2; }
for tool in awk sha256sum cmp date sleep; do
    command -v "$tool" >/dev/null 2>&1 || { printf 'Missing tool: %s\n' "$tool" >&2; exit 2; }
done
frames=${FRAMES:-1700}
profile_from=${PROFILE_FROM:-1400}
order=${BUDGET_ORDER:-32 64 96 128}
min_available=${MIN_AVAILABLE_KIB:-32768}
max_seconds=${MAX_SECONDS:-600}
for number in "$frames" "$profile_from" "$min_available" "$max_seconds"; do
    case "$number" in ''|*[!0-9]*) printf 'Numeric settings must be nonnegative integers\n' >&2; exit 2;; esac
done
if ! awk -v f="$frames" -v p="$profile_from" -v t="$max_seconds" 'BEGIN{exit !(f>=1700 && f<=2147483647 && p>=1 && p<=f && f-p+1>=300 && t>0)}'; then
    printf 'Require FRAMES>=1700 and at least 300 measured frames, positive timeout\n' >&2; exit 2
fi
seen=' '
for budget in $order; do
    case "$budget" in 32|64|96|128) ;; *) printf 'Budget order must contain 32 64 96 128\n' >&2; exit 2;; esac
    case "$seen" in *" $budget "*) printf 'Duplicate budget\n' >&2; exit 2;; esac
    seen="$seen$budget "
done
for budget in 32 64 96 128; do
    case "$seen" in *" $budget "*) ;; *) printf 'All four budgets are required\n' >&2; exit 2;; esac
done
out=${RESULTS_DIR:-$app_dir/cache-benchmark-$(date +%Y%m%d-%H%M%S)-$$}
mkdir "$out" || exit 2
out=$(CDPATH= cd -- "$out" && pwd) || exit 2
if [ "${SDL_VIDEODRIVER+x}" != x ]; then SDL_VIDEODRIVER=mali; fi
export SDL_VIDEODRIVER
cd "$app_dir" || exit 1
{
    date
    uname -a
    printf 'workload=coupled-unpaced textured frame-VBO frames=%s profile_from=%s order=%s\n' "$frames" "$profile_from" "$order"
    printf 'SDL_VIDEODRIVER=%s minimum_available_kib=%s timeout_s=%s\n' "$SDL_VIDEODRIVER" "$min_available" "$max_seconds"
    sha256sum "$binary"
    env | awk '/^(ENG_TEX_|PROPCYCL_)/'
} > "$out/manifest.txt"
pid=''
trap 'if [ -n "$pid" ]; then kill "$pid" 2>/dev/null || :; fi; exit 130' INT TERM HUP
sample_memory() {
    rss=NA;hwm=NA
    if [ -n "$pid" ] && [ -r "/proc/$pid/status" ]; then
        values=$(awk '/^VmRSS:/{r=$2}/^VmHWM:/{h=$2}END{print r?r:"NA",h?h:"NA"}' "/proc/$pid/status" 2>/dev/null) || values='NA NA'
        read -r rss hwm <<EOF
$values
EOF
    fi
    values=$(awk '/^MemAvailable:/{a=$2;have_a=1}/^CmaFree:/{c=$2;have_c=1}END{print have_a?a:"NA",have_c?c:"NA"}' /proc/meminfo 2>/dev/null) || values='NA NA'
    read -r available cma <<EOF
$values
EOF
    printf '%s\t%s\t%s\t%s\t%s\n' "$seconds" "$rss" "$hwm" "$available" "$cma" >> "$memory_file"
}
for budget in $order; do
    printf 'Running %s MiB: frames %s..%s\n' "$budget" "$profile_from" "$frames"
    prefix=$out/budget-$budget
    memory_file=$prefix-memory.tsv
    printf 'seconds\tRSS_kib\tHWM_kib\tMemAvailable_kib\tCmaFree_kib\n' > "$memory_file"
    seconds=0;sample_memory
    reason=completed
    if [ "$available" != NA ] && [ "$available" -lt "$min_available" ]; then
        printf 'Preflight memory guard: %s KiB available\n' "$available" > "$prefix.log"
        : > "$prefix.json";printf '125\n' > "$prefix.exit";continue
    fi
    "$binary" extracted --frames "$frames" --coin 180 --gas 200 \
        --steer 300:2304 --steer 700:2048 --timing coupled --unpaced \
        --render-mode textured --vbo-upload frame --texture-budget-mib "$budget" \
        --profile-from "$profile_from" --profile-stages --profile-memory \
        > "$prefix.json" 2> "$prefix.log" &
    pid=$!
    while kill -0 "$pid" 2>/dev/null; do
        sample_memory
        if [ "$available" != NA ] && [ "$available" -lt "$min_available" ]; then reason=memory_guard;break;fi
        if [ "$seconds" -ge "$max_seconds" ]; then reason=timeout;break;fi
        sleep 1
        seconds=$((seconds+1))
    done
    if [ "$reason" != completed ]; then
        printf '\n[BENCHMARK] stopped: %s\n' "$reason" >> "$prefix.log"
        kill "$pid" 2>/dev/null || :
        sleep 1
        kill -9 "$pid" 2>/dev/null || :
    fi
    if wait "$pid"; then rc=0;else rc=$?;fi
    pid='';sample_memory
    printf '%s\n' "$rc" > "$prefix.exit"
    printf '[BENCHMARK] exit=%s reason=%s\n' "$rc" "$reason" >> "$prefix.log"
    printf 'Budget %s exit=%s reason=%s\n' "$budget" "$rc" "$reason"
done
trap - INT TERM HUP
valid_state() {
    [ "$(cat "$1.exit")" = 0 ] && awk -v f="$frames" 'NR==1{ok=($0~/^\{"schema":1,/ && index($0,"\"frames\":" f ",") && $0~/"traps":0,"unmapped":0,"romwrites":0,/ && $0~/"dsp_fault":false,"sound_fault":false\}$/)}END{exit !(NR==1 && ok)}' "$1.json"
}
reference=$out/budget-64
reference_ok=false
if valid_state "$reference"; then reference_ok=true;fi
printf 'budget_mib\tframe_ms\tfps\tsimulation_ms\tscene_ms\tlookup_decode_ms\ttexture_upload_ms\tsubmission_ms\tswap_ms\tvertex_upload_ms\trender_other_ms\thits\tmisses\tevictions\ttexture_uploads\tGPU_peak_mib\tGPU_range_frame_peak_bytes\tRSS_peak_mib\tMemAvailable_min_mib\tCmaFree_min_mib\tstate_sha256\tstate\tstatus\n' > "$out/summary.tsv"
failed=0
for budget in 32 64 96 128; do
    prefix=$out/budget-$budget
    state=INVALID;hash=NA
    if valid_state "$prefix"; then
        hash=$(sha256sum "$prefix.json" | awk '{print $1}')
        if [ "$reference_ok" = true ]; then
            if cmp -s "$prefix.json" "$reference.json"; then state=SAME;else state=DIFF;fi
        else state=BASEFAIL;fi
    fi
    if ! awk -v budget="$budget" -v expected_samples="$((frames-profile_from+1))" \
        -v expected_range="$profile_from..$frames" -v exit_code="$(cat "$prefix.exit")" \
        -v state_status="$state" -v state_hash="$hash" -f "$summary_awk" \
        "$prefix.log" "$prefix-memory.tsv" >> "$out/summary.tsv";then failed=1;fi
done
awk -F '\t' 'NR==1{printf "MiB  frame_ms     FPS  lookup_ms  upload_ms    misses     evict  GPUpeak  RSSpeak  MemMin  state/status\n";next}
{printf "%3s %9s %7s %10s %10s %9s %9s %8s %8s %7s  %s/%s\n",$1,$2,$3,$6,$7,$13,$14,$16,$18,$19,$22,$23}' "$out/summary.tsv"
printf 'Detailed comparison, logs, memory samples and state hashes: %s\n' "$out"
exit "$failed"
