"""P1 private build instrumentation/ownership transformations (not gameplay)."""
def header(s):
 return s.replace('    void *native_payload;', '    double p1_sim_ms,p1_block_ms,p1_audio_ratio; unsigned p1_underruns,p1_depth;\n    void *native_payload;')
def presentation(s):
 s=s.replace('    double cpu = stamp(CLOCK_THREAD_CPUTIME_ID);\n    double serial_cpu_before', '    double p1_sim_end=now();\n    double cpu = stamp(CLOCK_THREAD_CPUTIME_ID);\n    double serial_cpu_before')
 s=s.replace('    metadata_lock();\n    last_frame = frame;', '    double p1_block=0;\n    metadata_lock();\n    last_frame = frame;')
 s=s.replace('while(slot<0&&!worker_failed){metadata_wait();', 'while(slot<0&&!worker_failed){double b=now();metadata_wait();p1_block+=now()-b;')
 s=s.replace('snapshot->native_tag=0;', 'snapshot->p1_sim_ms=(p1_sim_end-wall_last)*1000;snapshot->p1_block_ms=p1_block*1000;\n        snapshot->p1_audio_ratio=rr_p1_audio_ratio();rr_audio_output_stats p1_audio;rr_audio_output_status(&p1_audio);snapshot->p1_underruns=p1_audio.underruns;\n        snapshot->native_tag=0;')
 s=s.replace('if (depth > queue_peak)', 'unsigned occupied=0;for(int i=0;i<SLOTS;i++)occupied+=state[i]!=FREE;snapshot->p1_depth=occupied;\n        if (depth > queue_peak)')
 s=s.replace('            fputc(\'\\n\',profile_csv);', '            fprintf(profile_csv,",%u,%.9f,%.9f,%.9f,%.9f,%.9f,%u,%u",snapshot->native_tag,snapshot->p1_sim_ms,snapshot->native_walk_ms,snapshot->native_compile_ms,snapshot->p1_block_ms,snapshot->p1_audio_ratio,snapshot->p1_underruns,snapshot->p1_depth);\n            fputc(\'\\n\',profile_csv);')
 s=s.replace('        fputc(\'\\n\',profile_csv);', '        fputs(",payload_native,simulation_ms,native_walk_ms,native_compile_ms,simulation_blocked_ms,resampler_ratio,audio_underruns,occupied_slots",profile_csv);\n        fputc(\'\\n\',profile_csv);',1) if False else s
 # Header location is distinct from row fputc following the new fprintf.
 s=s.replace('        fputc(\'\\n\',profile_csv);\n    }\n    slots =', '        fputs(",payload_native,simulation_ms,native_walk_ms,native_compile_ms,simulation_blocked_ms,resampler_ratio,audio_underruns,occupied_slots",profile_csv);\n        fputc(\'\\n\',profile_csv);\n    }\n    slots =')
 s='#include "rr_sound.h"\n'+s
 return s
