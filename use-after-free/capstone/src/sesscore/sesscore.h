/* sesscore.h — public API of sesscore.dll.
 *
 * The session store: a handle table (id -> Session*) plus open/use/close/stat,
 * a "most-recent session" fast path, and a deferred-write batch. Every Revenant
 * frontend links it.
 */
#ifndef REVENANT_SESSCORE_H
#define REVENANT_SESSCORE_H

#include <stddef.h>
#include <stdint.h>
#include "../common/handler.h"

#ifdef SESSCORE_EXPORTS
#define SESSCORE_API __declspec(dllexport)
#else
#define SESSCORE_API __declspec(dllimport)
#endif

typedef struct Session {
    int            id;
    int            type;
    SessHandlerFn  handler;     /* set at OPEN from a handler plugin */
    unsigned char *data;        /* owned heap buffer                  */
    uint32_t       len;
    uint32_t       cap;
} Session;

#define SESS_MAX      256
#define SESS_RECENT   0xFFFF     /* handle sentinel: operate on the most-recent session */
#define SESS_MAX_DATA (1u << 20) /* cap on a session's data buffer */

typedef struct SessTable {
    Session *slots[SESS_MAX];   /* handle id -> Session*  */
    int      count;
} SessTable;

#define SESS_OK   0
#define SESS_ERR -1

SESSCORE_API SessTable *SessNew(void);
SESSCORE_API void       SessTableFree(SessTable *t);

SESSCORE_API int      SessOpen(SessTable *t, int type, SessHandlerFn handler,
                               const unsigned char *init, uint32_t len);
/* Resolve a handle. id == SESS_RECENT returns the most-recently-used session.
 * A normal id returns slots[id] (NULL once CLOSE has cleared it). */
SESSCORE_API Session *SessGet(SessTable *t, int id);

SESSCORE_API int  SessUseRun(SessTable *t, int id, const unsigned char *arg, uint32_t len,
                             char *out, uint32_t out_cap);
SESSCORE_API int  SessUseSet(SessTable *t, int id, const unsigned char *arg, uint32_t len);
SESSCORE_API int  SessUseAppend(SessTable *t, int id, const unsigned char *arg, uint32_t len);

SESSCORE_API int  SessClose(SessTable *t, int id);   /* frees the session and clears its handle */

SESSCORE_API int  SessStat(SessTable *t, int verbose, char *out, size_t out_cap);

/* deferred batch */
SESSCORE_API void SessDefer(SessTable *t, int id);
SESSCORE_API int  SessFlush(SessTable *t, const unsigned char *arg, uint32_t len);

#endif /* REVENANT_SESSCORE_H */
