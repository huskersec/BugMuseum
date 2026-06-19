/* sesscore.c — Revenant session store.
 *
 * A handle table (id -> Session*), the OPEN/USE/CLOSE/STAT operations, a
 * most-recently-used fast path, and a deferred-write batch.
 *
 * Build: cl /LD /DSESSCORE_EXPORTS sesscore.c   (see ../../build.ps1)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "sesscore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* most-recently-touched session, kept for the SESS_RECENT fast path and STAT */
static Session *g_recent;

/* deferred-write batch: data pointers captured from sessions for a later flush */
#define DEFER_MAX 64
static struct {
    unsigned char *data[DEFER_MAX];
    uint32_t       len[DEFER_MAX];
    int            n;
} g_batch;

/* ---- lifecycle ----------------------------------------------------------- */

SESSCORE_API SessTable *SessNew(void) {
    return (SessTable *)calloc(1, sizeof(SessTable));
}

static void free_session(Session *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

SESSCORE_API void SessTableFree(SessTable *t) {
    /* Only the table is reclaimed here; live sessions belong to the process for
     * its lifetime. */
    free(t);
}

SESSCORE_API int SessOpen(SessTable *t, int type, SessHandlerFn handler,
                          const unsigned char *init, uint32_t len) {
    if (!t || !handler) return SESS_ERR;
    int id = -1;
    for (int i = 0; i < SESS_MAX; i++) { if (!t->slots[i]) { id = i; break; } }
    if (id < 0) return SESS_ERR;
    if (len > SESS_MAX_DATA) len = SESS_MAX_DATA;

    Session *s = (Session *)calloc(1, sizeof(Session));
    if (!s) return SESS_ERR;
    s->id      = id;
    s->type    = type;
    s->handler = handler;
    s->cap     = len < 32 ? 32 : len;
    s->data    = (unsigned char *)malloc(s->cap);
    if (!s->data) { free(s); return SESS_ERR; }
    if (len) memcpy(s->data, init, len);
    s->len = len;

    t->slots[id] = s;
    t->count++;
    g_recent = s;
    return id;
}

/* ---- lookup -------------------------------------------------------------- */

SESSCORE_API Session *SessGet(SessTable *t, int id) {
    if (id == SESS_RECENT) return g_recent;     /* fast path: the cached recent session */
    if (!t || id < 0 || id >= SESS_MAX) return NULL;
    Session *s = t->slots[id];
    if (s) g_recent = s;                        /* remember the most-recent for the fast path/STAT */
    return s;
}

/* ---- use ----------------------------------------------------------------- */

SESSCORE_API int SessUseRun(SessTable *t, int id, const unsigned char *arg, uint32_t len,
                            char *out, uint32_t out_cap) {
    Session *s = SessGet(t, id);
    if (!s) return SESS_ERR;
    return s->handler(arg, len, out, out_cap);   /* invoke the session's handler */
}

SESSCORE_API int SessUseSet(SessTable *t, int id, const unsigned char *arg, uint32_t len) {
    Session *s = SessGet(t, id);
    if (!s) return SESS_ERR;
    uint32_t n = len < s->cap ? len : s->cap;
    memcpy(s->data, arg, n);
    s->len = n;
    return SESS_OK;
}

SESSCORE_API int SessUseAppend(SessTable *t, int id, const unsigned char *arg, uint32_t len) {
    Session *s = SessGet(t, id);
    if (!s) return SESS_ERR;
    unsigned char *old = s->data;          /* current data buffer */
    uint32_t need = s->len + len;
    if (need > s->cap) {
        unsigned char *grown = (unsigned char *)realloc(s->data, need);
        if (!grown) return SESS_ERR;
        s->data = grown;
        s->cap  = need;
    }
    memcpy(old + s->len, arg, len);         /* write at the saved buffer + offset */
    s->len = need;
    return SESS_OK;
}

/* ---- close --------------------------------------------------------------- */

SESSCORE_API int SessClose(SessTable *t, int id) {
    if (!t || id < 0 || id >= SESS_MAX) return SESS_ERR;
    Session *s = t->slots[id];
    if (!s) return SESS_ERR;
    free_session(s);            /* release s and s->data */
    t->slots[id] = NULL;        /* the handle table is kept correct... */
    t->count--;
    /* (g_recent and any deferred-batch entries are not touched here) */
    return SESS_OK;
}

/* ---- stat ---------------------------------------------------------------- */

SESSCORE_API int SessStat(SessTable *t, int verbose, char *out, size_t out_cap) {
    int w = _snprintf(out, out_cap, "sessions=%d", t ? t->count : 0);
    if (w < 0) w = 0;
    if (verbose && g_recent && (size_t)w < out_cap) {
        _snprintf(out + w, out_cap - (size_t)w, " recent=#%d len=%u",
                  g_recent->id, g_recent->len);
    }
    if (out_cap) out[out_cap - 1] = '\0';
    return SESS_OK;
}

/* ---- deferred batch ------------------------------------------------------ */

SESSCORE_API void SessDefer(SessTable *t, int id) {
    Session *s = SessGet(t, id);
    if (!s || g_batch.n >= DEFER_MAX) return;
    g_batch.data[g_batch.n] = s->data;      /* capture the data pointer for later */
    g_batch.len[g_batch.n]  = s->len;
    g_batch.n++;
}

SESSCORE_API int SessFlush(SessTable *t, const unsigned char *arg, uint32_t len) {
    (void)t;
    for (int i = 0; i < g_batch.n; i++) {
        uint32_t n = len < g_batch.len[i] ? len : g_batch.len[i];
        memcpy(g_batch.data[i], arg, n);    /* write into each captured buffer */
    }
    g_batch.n = 0;
    return SESS_OK;
}
