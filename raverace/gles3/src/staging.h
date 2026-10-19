/* Owned slots retain their own last packed bytes. Only an identical prefix may
 * be reused: after any layout/content change, all following regions are copied. */
static void preparation_stage_begin(DynamicCacheSlot *s)
{
    s->prior_uploads = s->uploads;
    s->reuse_prefix = true;
    s->copied_bytes = s->metadata_bytes = 0;
    memset(s, 0, offsetof(DynamicCacheSlot, upload));
}
static void preparation_stage(DynamicCacheSlot *s, unsigned u, const DynamicCacheUpload *next, const uint8_t *source)
{
    const DynamicCacheUpload *old = s->upload + u;
    if (u >= s->prior_uploads || old->entry != next->entry || old->generation != next->generation ||
        old->offset != next->offset || old->x != next->x || old->y != next->y || old->w != next->w || old->h != next->h)
        s->reuse_prefix = false;
    if (!s->reuse_prefix) {
        s->upload[u] = *next;
        s->metadata_bytes += sizeof *next;
        for (unsigned y = 0; y < next->h; y++)
            memcpy(
                s->pens + next->offset + y * next->w, source + (next->y + y) * DYNAMIC_CACHE_SIDE + next->x, next->w);
        s->copied_bytes += (uint64_t)next->w * next->h;
    }
}
