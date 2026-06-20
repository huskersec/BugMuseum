/* spoolcore.h — public API of spoolcore.dll.
 *
 * The job store: a job table (id -> Job*) plus a ready queue, submit/render/
 * append/cancel/recycle/dup/flush, a most-recent "active job" fast path, and a
 * render pump. Every Encore frontend links it.
 */
#ifndef ENCORE_SPOOLCORE_H
#define ENCORE_SPOOLCORE_H

#include <stddef.h>
#include <stdint.h>
#include "../common/filter.h"

#ifdef SPOOLCORE_EXPORTS
#define SPOOLCORE_API __declspec(dllexport)
#else
#define SPOOLCORE_API __declspec(dllimport)
#endif

enum job_state { JOB_QUEUED = 0, JOB_RENDERING, JOB_DONE, JOB_CANCELLED };

typedef struct Job {
    int            id;
    int            fmt;
    int            state;
    FilterFn       filter;      /* set at SUBMIT from a filter plugin */
    unsigned char *data;        /* owned document buffer              */
    uint32_t       len;
    uint32_t       cap;
    unsigned char *meta;        /* owned metadata buffer (job header) */
    uint32_t       meta_len;
    struct Job    *qnext;       /* ready-queue link                   */
} Job;

#define SPOOL_MAX      256
#define SPOOL_RECENT   0xFFFF     /* job id sentinel: the most-recent job */
#define ENC_MAX_DOC    (1u << 20) /* cap on a job's document buffer       */

typedef struct SpoolTable {
    Job *slots[SPOOL_MAX];      /* job id -> Job*       */
    Job *qhead, *qtail;         /* ready queue          */
    int  count;
} SpoolTable;

#define SPOOL_OK   0
#define SPOOL_ERR -1

SPOOLCORE_API SpoolTable *SpoolNew(void);
SPOOLCORE_API void        SpoolTableFree(SpoolTable *t);

SPOOLCORE_API int   JobSubmit(SpoolTable *t, int fmt, FilterFn filter,
                              const unsigned char *init, uint32_t len);
/* Resolve a job id. id == SPOOL_RECENT returns the most-recently-touched job.
 * A normal id returns slots[id] (NULL once it has been retired). */
SPOOLCORE_API Job  *JobGet(SpoolTable *t, int id);

SPOOLCORE_API int   JobRender(SpoolTable *t, int id, char *out, uint32_t out_cap);
SPOOLCORE_API int   JobAppend(SpoolTable *t, int id, const unsigned char *arg, uint32_t len);
SPOOLCORE_API int   JobCancel(SpoolTable *t, int id);
SPOOLCORE_API int   JobRecycle(SpoolTable *t, int id);
SPOOLCORE_API int   JobDup(SpoolTable *t, int id);

SPOOLCORE_API int   JobStatus(SpoolTable *t, int verbose, char *out, size_t out_cap);

/* the render pump */
SPOOLCORE_API Job  *JobDequeue(SpoolTable *t);
SPOOLCORE_API int   JobWork(SpoolTable *t, Job *j);   /* render one dequeued job, then retire it */
SPOOLCORE_API int   JobFlush(SpoolTable *t);          /* drain the ready queue through JobWork    */

SPOOLCORE_API int   JobRetire(SpoolTable *t, int id); /* release a job and free its slot */

#endif /* ENCORE_SPOOLCORE_H */
