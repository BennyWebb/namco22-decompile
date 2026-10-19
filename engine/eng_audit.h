/* Diagnostic-only main-thread wall/CPU attribution. No GPU synchronisation. */
#ifndef ENG_AUDIT_H
#define ENG_AUDIT_H
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
typedef enum { AUD_SIM, AUD_SCENE, AUD_BACKEND, AUD_LOOKUP, AUD_UPLOAD,
    AUD_SUBMIT, AUD_VERTEX, AUD_DRAW, AUD_SWAP, AUD_WAIT, AUD_DECODE, AUD_COUNT } eng_audit_stage;
typedef struct { int parent, stage; double wall, cpu; } eng_audit_scope;
extern bool eng_audit_enabled, eng_audit_no_draw;
extern bool eng_audit_flat_fragment;
extern bool eng_audit_detailed;
extern int eng_audit_divisor;
extern double eng_audit_wall[AUD_COUNT], eng_audit_cpu[AUD_COUNT];
extern double eng_audit_inclusive_wall[AUD_COUNT], eng_audit_inclusive_cpu[AUD_COUNT];
bool eng_audit_available(void);
eng_audit_scope eng_audit_begin_impl(eng_audit_stage stage);
void eng_audit_end_impl(eng_audit_scope scope);
void eng_audit_reset(void);
double eng_audit_cpu_now(void);
void eng_audit_pmu_start(void);
void eng_audit_pmu_report(void);
#ifdef RR_PIPELINE_AUDIT
static inline eng_audit_scope eng_audit_begin(eng_audit_stage stage)
{
    if (!eng_audit_enabled || (!eng_audit_detailed && (stage==AUD_DRAW || stage==AUD_LOOKUP)))
        return (eng_audit_scope){.stage=-1};
    return eng_audit_begin_impl(stage);
}
static inline void eng_audit_end(eng_audit_scope scope)
{ if(eng_audit_enabled && scope.stage>=0) eng_audit_end_impl(scope); }
#define AUDIT_NO_DRAW eng_audit_no_draw
#define AUDIT_DIVISOR eng_audit_divisor
#define AUDIT_FLAT eng_audit_flat_fragment
#else
static inline eng_audit_scope eng_audit_begin(eng_audit_stage stage)
{ (void)stage;return (eng_audit_scope){0}; }
static inline void eng_audit_end(eng_audit_scope scope) { (void)scope; }
#define AUDIT_NO_DRAW false
#define AUDIT_DIVISOR 1
#define AUDIT_FLAT false
#endif
#endif
