/* ---------------------------------------------------------- recorder ---- */
static FILE *rec_f, *rep_f;
static uint32_t rep_next_frame = UINT32_MAX;
static unsigned rep_in, rep_st, rep_gas, rep_br;
static uint16_t last[4];
static unsigned pending_markers, marker_number;
static bool rec_first, stream_ok = true;
static uint32_t previous_event;
static bool have_event;

static void rep_read(void)
{
    char line[256], tail;
    rep_next_frame = UINT32_MAX;
    while (rep_f && fgets(line, sizeof line, rep_f)) {
        char *p=line; while (isspace((unsigned char)*p)) p++;
        if (*p=='#' || *p=='\n' || !*p) continue;
        unsigned f, in, st, gas, br;
        if (sscanf(p,"F %u %x %x %x %x %c",&f,&in,&st,&gas,&br,&tail)!=5 ||
            f>=INT32_MAX || in>65535 || st>4095 || gas>4095 || br>4095 ||
            (have_event && f<=previous_event)) {
            fprintf(stderr,"[INPUT] invalid/non-increasing replay event: %s",line);
            stream_ok=false; return;
        }
        rep_next_frame=f; rep_in=in; rep_st=st; rep_gas=gas; rep_br=br;
        previous_event=f; have_event=true; return;
    }
    if (rep_f && ferror(rep_f)) stream_ok=false;
}

bool rr_input_replay_open(const char *path)
{
    if (rep_f) fclose(rep_f);
    stream_ok=true; have_event=false;
    rep_f = fopen(path, "r");
    if (!rep_f) { fprintf(stderr, "[INPUT] cannot open %s\n", path); return false; }
    rep_read();
    if (rep_next_frame == UINT32_MAX) { fprintf(stderr, "[INPUT] %s holds no input events\n", path); fclose(rep_f); rep_f=NULL; return false; }
    fprintf(stderr, "[INPUT] replaying %s\n", path);
    return true;
}
bool rr_input_replaying(void) { return rep_f != NULL; }

bool rr_input_record_start(const char *path)
{
    if (rec_f) return true;
    rec_f = fopen(path, "wx");
    if (!rec_f) { fprintf(stderr, "[INPUT] cannot write %s\n", path); return false; }
    fprintf(rec_f, "# %s input recording: F frame inputs steer gas brake (hex), on change\n", g_rr_game->title);
    rec_first = true;pending_markers=marker_number=0;
    fprintf(stderr, "[INPUT] recording to %s\n", path);
    return true;
}
void rr_input_record_stop(void)
{
    if (rec_f) { if (ferror(rec_f)) stream_ok=false; if (fclose(rec_f)) stream_ok=false; rec_f = NULL; fprintf(stderr, "[INPUT] recording stopped\n"); }
}
bool rr_input_recording(void) { return rec_f != NULL; }

/* once per frame, after the host has polled: replay overrides, recorder logs */
void rr_input_frame(uint32_t frame)
{
    if (rep_f) {
        while (stream_ok && rep_next_frame != UINT32_MAX && rep_next_frame <= frame) {
            g_hw.inputs = (uint16_t)rep_in; g_hw.steer = (uint16_t)rep_st;
            g_hw.gas = (uint16_t)rep_gas; g_hw.brake = (uint16_t)rep_br;
            rep_read();
        }
    }
    if (rec_f) {
        while(pending_markers){fprintf(rec_f,"# MARK %u %u\n",frame,++marker_number);pending_markers--;}
        uint16_t cur[4] = { g_hw.inputs, g_hw.steer, g_hw.gas, g_hw.brake };
        if (rec_first || memcmp(cur, last, sizeof cur)) {
            fprintf(rec_f, "F %u %04X %03X %03X %03X\n", frame, cur[0], cur[1], cur[2], cur[3]);
            memcpy(last, cur, sizeof cur);
            rec_first = false;
        }
    }
}


bool rr_input_stream_ok(void) { return stream_ok && (!rec_f || !ferror(rec_f)); }
void rr_input_stream_close(void) { rr_input_record_stop(); if(rep_f) {fclose(rep_f);rep_f=NULL;} }

void rr_input_record_marker(void) {if(rec_f && pending_markers<16)pending_markers++;}
