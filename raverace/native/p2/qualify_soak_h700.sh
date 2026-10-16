#!/bin/sh
# Return 1 when any native clock sample departs or CPU cooling is active.
set -eu
run=$1;frequency=$2
: > "$run/native-windows.txt"
for file in "$run"/lap-*/thread.csv;do
 awk -F, 'NR==1{for(i=1;i<=NF;i++){if($i=="frame")f=i;if($i=="presented")p=i;if($i=="publication_age")a=i;if($i=="simulation_ms")s=i}}NR>1&&$f==1800{lo=$p-$a-$s/1000}NR>1&&$f==6006{printf "%.9f %.9f\n",lo,$p}' "$file" >> "$run/native-windows.txt"
done
awk -v target="$frequency" '
 NR==FNR{lo[++n]=$1;hi[n]=$2;next}
 /^SAMPLE /{t=$2;active=0;for(i=1;i<=n;i++)if(lo[i]<=t&&t<=hi[i])active=1;if(active)samples++;next}
 active&&/^FILE \/sys\/devices\/system\/cpu\/cpu[0-3]\/cpufreq\/scaling_cur_freq$/{getline;cpu_reads++;if($1!=target){print "FREQUENCY",t,$1,"expected",target;bad++}next}
 /^FILE \/sys\/class\/thermal\/cooling_device0\/cur_state$/{getline;if($1!=0){print "CPU_COOLING",t,$1;bad++}next}
 END{if(n==0||samples==0||cpu_reads!=samples*4){print "INVALID_COVERAGE",n,samples,cpu_reads;bad++}print "native_samples",samples,"flagged_values",bad+0;exit bad>0}
' "$run/native-windows.txt" "$run/memory.txt" > "$run/clock-qualification.txt"
