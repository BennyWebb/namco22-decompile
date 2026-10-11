/* CPU-only raster preparation shared by the desktop and GLES backends.
 * Requires geo_quad, g_scene_x0/x1 and ENG_SCREEN_H declarations. */
static int g_sort_tie_emit;
typedef struct { int32_t zsort; int order; int idx; } quad_key;
static int zcmp(const void *a, const void *b)
{
    const quad_key *qa = (const quad_key *)a, *qb = (const quad_key *)b;
    if (qa->zsort != qb->zsort)
        return (qa->zsort > qb->zsort) ? -1 : 1;   /* far (large) first */
    /* TIES DRAW IN REVERSE SUBMISSION ORDER -- the FIRST quad emitted at a
     * zsort ends up ON TOP. MAME's namcos22_renderer::new_scenenode PREPENDS
     * a leaf to an occupied radix bucket (`leaf->next = node`), and
     * render_scene_nodes walks each bucket from its head, so the most recent
     * insertion is drawn first (Prop Cycle register row 175). */
    if (g_sort_tie_emit) return (qa->order < qb->order) ? -1 : (qa->order > qb->order) ? 1 : 0;
    return (qa->order > qb->order) ? -1 : (qa->order < qb->order) ? 1 : 0;
}

/* Sorts small keys, then moves each quad once. A geo_quad is ~1 KB and the Windows C runtime's qsort swaps elements
 * a byte at a time: sorting the quads themselves cost ~9 ms a frame there (glibc's merge sort hid it on Linux).
 * `order` is unique per quad, so the order is total and the result is the same whatever the sort. */
void eng_quad_sort(geo_quad *buf, int n, int tie_emit)
{
    static quad_key *keys;
    static int cap;
    if (n < 2) return;
    if (n > cap) {
        quad_key *k = realloc(keys, (size_t)n * sizeof *k);
        if (!k) {
#ifdef RR_GLES2
            fprintf(stderr, "[GLES2] sort allocation failed: %zu bytes\n", (size_t)n * sizeof *k);
            exit(2);
#else
            return;
#endif
        }
        keys = k; cap = n;
    }
    for (int i = 0; i < n; i++) { keys[i].zsort = buf[i].zsort; keys[i].order = buf[i].order; keys[i].idx = i; }
    g_sort_tie_emit = tie_emit;
    qsort(keys, (size_t)n, sizeof keys[0], zcmp);
    /* apply the permutation in place, one cycle at a time: slot j takes the quad from keys[j].idx */
    for (int i = 0; i < n; i++) {
        if (keys[i].idx == i) continue;
        geo_quad held = buf[i];
        int j = i;
        for (;;) {
            const int k = keys[j].idx;
            keys[j].idx = j;
            if (k == i) { buf[j] = held; break; }
            buf[j] = buf[k];
            j = k;
        }
    }
}

/* ---- screen-space polygon clip -----------------------------------------
 * geo_hw's near-plane clip puts intersection vertices on z=1, which the
 * oracle itself notes "project far off-canvas" -- coordinates in the
 * billions, saturated to INT32. A hardware rasteriser simply does not write
 * those pixels; GL cannot rasterise them at all.
 *
 * The old workaround clamped each vertex to +/-4096. That bounds the number
 * but MOVES the vertex, so the edge slopes change and the quad becomes a
 * screen-spanning wedge -- the "polygons exploding now and then" artifact,
 * appearing precisely on partially-clipped quads.
 *
 * Clipping the polygon against the screen rectangle preserves the on-screen
 * shape exactly. Attributes interpolate linearly in SCREEN space, which is
 * correct for s=u/z, t=v/z, w=1/z (that is what makes the texturing
 * perspective-correct) and for the shade value. */
typedef struct { float x, y, s, t, w, bri; } geo_sv;

static int clip_edge(const geo_sv *in, int n, geo_sv *out, int axis,
                     float limit, int keep_greater)
{
    int m = 0;
    for (int i = 0; i < n && m < 30; i++) {
        const geo_sv *a = &in[i], *b = &in[(i + 1) % n];
        float av = axis ? a->y : a->x, bv = axis ? b->y : b->x;
        int ain = keep_greater ? (av >= limit) : (av <= limit);
        int bin = keep_greater ? (bv >= limit) : (bv <= limit);
        if (ain) out[m++] = *a;
        if (ain != bin && m < 30) {
            float d = bv - av;
            float t = (d != 0.0f) ? (limit - av) / d : 0.0f;
            geo_sv v;
            v.x = a->x + (b->x - a->x) * t;
            v.y = a->y + (b->y - a->y) * t;
            v.s = a->s + (b->s - a->s) * t;
            v.t = a->t + (b->t - a->t) * t;
            v.w = a->w + (b->w - a->w) * t;
            v.bri = a->bri + (b->bri - a->bri) * t;
            out[m++] = v;
        }
    }
    return m;
}

static int clip_to_screen(const geo_sv *in, int n, geo_sv *out)
{
    geo_sv a[32], b[32];
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) a[i] = in[i];
    n = clip_edge(a, n, b, 0, g_scene_x0, 1);           if (n < 3) return 0;
    n = clip_edge(b, n, a, 0, g_scene_x1, 0);           if (n < 3) return 0;
    n = clip_edge(a, n, b, 1, 0.0f, 1);                 if (n < 3) return 0;
    n = clip_edge(b, n, a, 1, (float)ENG_SCREEN_H, 0); if (n < 3) return 0;
    for (int i = 0; i < n && i < 32; i++) out[i] = a[i];
    return n;
}


/* The same fan expansion is used by desktop and GLES submission. */
static int eng_fan_index(int triangle, int corner) { return corner == 0 ? 0 : triangle + corner - 1; }
