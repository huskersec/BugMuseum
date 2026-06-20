/* spoolcore.c — Encore job store.
 *
 * A job table (id -> Job*), a ready queue, the submit/render/append/cancel/
 * recycle/dup operations, a most-recently-touched fast path, and the render
 * pump that drains the queue.
 *
 * Build: cl /LD /DSPOOLCORE_EXPORTS spoolcore.c   (see ../../build.ps1)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "spoolcore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* most-recently-touched job, kept for the SPOOL_RECENT fast path and STATUS */
static Job *g_active;

/* the previous job's document buffer, kept so the next submit can reuse the
 * allocation instead of going back to the heap */
static unsigned char *g_scratch;

/* ---- queue helpers ------------------------------------------------------- */

static void enqueue(SpoolTable *t, Job *j) {
    j->qnext = NULL;
    if (t->qtail) t->qtail->qnext = j; else t->qhead = j;
    t->qtail = j;
}

SPOOLCORE_API Job *JobDequeue(SpoolTable *t) {
    if (!t) return NULL;
    Job *j = t->qhead;
    if (j) {
        t->qhead = j->qnext;
        if (!t->qhead) t->qtail = NULL;
        j->qnext = NULL;
    }
    return j;
}

/* ---- lifecycle ----------------------------------------------------------- */

SPOOLCORE_API SpoolTable *SpoolNew(void) {
    return (SpoolTable *)calloc(1, sizeof(SpoolTable));
}

static void unlink_queue(SpoolTable *t, Job *j) {
    Job *prev = NULL;
    for (Job *c = t->qhead; c; prev = c, c = c->qnext) {
        if (c == j) {
            if (prev) prev->qnext = c->qnext; else t->qhead = c->qnext;
            if (t->qtail == c) t->qtail = prev;
            return;
        }
    }
}

static void free_job(Job *j) {
    if (!j) return;
    if (g_active == j) g_active = NULL;     /* drop the fast-path cache if it named us */
    free(j->meta);
    free(j->data);
    free(j);
}

SPOOLCORE_API void SpoolTableFree(SpoolTable *t) {
    /* Only the table is reclaimed here; live jobs belong to the process for its
     * lifetime. */
    free(t);
}

static int alloc_slot(SpoolTable *t) {
    for (int i = 0; i < SPOOL_MAX; i++) if (!t->slots[i]) return i;
    return -1;
}

SPOOLCORE_API int JobSubmit(SpoolTable *t, int fmt, FilterFn filter,
                            const unsigned char *init, uint32_t len) {
    if (!t || !filter) return SPOOL_ERR;
    int id = alloc_slot(t);
    if (id < 0) return SPOOL_ERR;
    if (len > ENC_MAX_DOC) len = ENC_MAX_DOC;

    Job *j = (Job *)calloc(1, sizeof(Job));
    if (!j) return SPOOL_ERR;
    j->id     = id;
    j->fmt    = fmt;
    j->state  = JOB_QUEUED;
    j->filter = filter;
    j->cap    = len < 32 ? 32 : len;
    j->data   = (unsigned char *)malloc(j->cap);
    if (!j->data) { free(j); return SPOOL_ERR; }
    if (len) memcpy(j->data, init, len);
    j->len = len;

    /* a small metadata header travels with every job */
    j->meta_len = 16;
    j->meta     = (unsigned char *)malloc(j->meta_len);
    if (j->meta) { memset(j->meta, 0, j->meta_len); j->meta[0] = (unsigned char)fmt; }

    t->slots[id] = j;
    t->count++;
    enqueue(t, j);
    g_active = j;
    return id;
}

/* ---- lookup -------------------------------------------------------------- */

SPOOLCORE_API Job *JobGet(SpoolTable *t, int id) {
    if (id == SPOOL_RECENT) return g_active;    /* fast path: the cached active job */
    if (!t || id < 0 || id >= SPOOL_MAX) return NULL;
    Job *j = t->slots[id];
    if (j) g_active = j;                         /* remember the most-recent for the fast path/STATUS */
    return j;
}

/* ---- render -------------------------------------------------------------- */

SPOOLCORE_API int JobRender(SpoolTable *t, int id, char *out, uint32_t out_cap) {
    Job *j = JobGet(t, id);
    if (!j) return SPOOL_ERR;
    int rc = j->filter(j->data, j->len, out, out_cap);   /* render via the plugin */
    JobRetire(t, j->id);                                 /* the print is done -> retire it */
    return rc;
}

/* ---- append -------------------------------------------------------------- */

SPOOLCORE_API int JobAppend(SpoolTable *t, int id, const unsigned char *arg, uint32_t len) {
    Job *j = JobGet(t, id);
    if (!j) return SPOOL_ERR;
    uint32_t need = j->len + len;
    if (len > ENC_MAX_DOC || need > ENC_MAX_DOC) return SPOOL_ERR;  /* range-checked before any alloc */
    if (need > j->cap) {
        unsigned char *old   = j->data;
        unsigned char *grown = (unsigned char *)realloc(j->data, need);
        if (!grown) return SPOOL_ERR;
        j->data = grown;
        j->cap  = need;
        memcpy(j->data + j->len, arg, len);
        j->len  = need;
        if (grown != old) free(old);   /* realloc moved us; drop the old block */
        return SPOOL_OK;
    }
    memcpy(j->data + j->len, arg, len);
    j->len = need;
    return SPOOL_OK;
}

/* ---- cancel / recycle / dup --------------------------------------------- */

SPOOLCORE_API int JobCancel(SpoolTable *t, int id) {
    Job *j = JobGet(t, id);
    if (!j) return SPOOL_ERR;
    j->state = JOB_CANCELLED;   /* the pump drops it when it reaches the queue */
    return SPOOL_OK;
}

SPOOLCORE_API int JobRecycle(SpoolTable *t, int id) {
    Job *j = JobGet(t, id);
    if (!j) return SPOOL_ERR;
    free(g_scratch);            /* release the buffer we kept from last time */
    g_scratch = j->data;        /* keep this job's buffer to reuse next time  */
    return SPOOL_OK;
}

SPOOLCORE_API int JobDup(SpoolTable *t, int id) {
    Job *j = JobGet(t, id);
    if (!j) return SPOOL_ERR;
    int nid = alloc_slot(t);
    if (nid < 0) return SPOOL_ERR;

    Job *nj = (Job *)calloc(1, sizeof(Job));
    if (!nj) return SPOOL_ERR;
    nj->id     = nid;
    nj->fmt    = j->fmt;
    nj->state  = JOB_QUEUED;
    nj->filter = j->filter;
    nj->cap    = j->len < 32 ? 32 : j->len;
    nj->data   = (unsigned char *)malloc(nj->cap);     /* the document is copied */
    if (!nj->data) { free(nj); return SPOOL_ERR; }
    if (j->len) memcpy(nj->data, j->data, j->len);
    nj->len      = j->len;
    nj->meta     = j->meta;                            /* metadata is immutable: share it */
    nj->meta_len = j->meta_len;

    t->slots[nid] = nj;
    t->count++;
    enqueue(t, nj);
    g_active = nj;
    return nid;
}

/* ---- status -------------------------------------------------------------- */

SPOOLCORE_API int JobStatus(SpoolTable *t, int verbose, char *out, size_t out_cap) {
    int w = _snprintf(out, out_cap, "jobs=%d", t ? t->count : 0);
    if (w < 0) w = 0;
    if (verbose && g_active && (size_t)w < out_cap) {
        _snprintf(out + w, out_cap - (size_t)w, " active=#%d len=%u state=%d",
                  g_active->id, g_active->len, g_active->state);
    }
    if (out_cap) out[out_cap - 1] = '\0';
    return SPOOL_OK;
}

/* ---- render pump --------------------------------------------------------- */

SPOOLCORE_API int JobWork(SpoolTable *t, Job *j) {
    char out[256];
    int  id;
    if (!j) return SPOOL_ERR;
    id = j->id;
    if (j->state == JOB_CANCELLED) {
        free_job(j);            /* cancelled before we got to it: discard the job */
        goto done;
    }
    j->state = JOB_RENDERING;
    j->filter(j->data, j->len, out, sizeof out);   /* render via the plugin */
    j->state = JOB_DONE;
done:
    JobRetire(t, id);           /* mark the slot free now that we're finished */
    return SPOOL_OK;
}

SPOOLCORE_API int JobFlush(SpoolTable *t) {
    Job *j;
    while ((j = JobDequeue(t)) != NULL) JobWork(t, j);
    return SPOOL_OK;
}

SPOOLCORE_API int JobRetire(SpoolTable *t, int id) {
    if (!t || id < 0 || id >= SPOOL_MAX) return SPOOL_ERR;
    Job *j = t->slots[id];
    if (!j) return SPOOL_ERR;
    unlink_queue(t, j);         /* a retired job must not linger in the ready queue */
    free_job(j);                /* release the job and its buffers */
    t->slots[id] = NULL;        /* the job table is kept correct */
    t->count--;
    return SPOOL_OK;
}
