/* cfgcore.h — public API of cfgcore.dll.
 *
 * cfgcore is the shared engine: it parses .cprof bundles into an in-memory
 * profile, looks keys up, merges profiles, and provides a few serve-time
 * helpers used by the service frontend. Every Cascade frontend links it.
 */
#ifndef CASCADE_CFGCORE_H
#define CASCADE_CFGCORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef CFGCORE_EXPORTS
#define CFGCORE_API __declspec(dllexport)
#else
#define CFGCORE_API __declspec(dllimport)
#endif

/* In-memory entry. Raw value bytes are kept for str/wstr/blob/template;
 * integers are decoded into ival. */
typedef struct CfgEntry {
    char            *name;       /* NUL-terminated, heap                 */
    int              type;       /* enum cprof_type                      */
    uint32_t         value_len;  /* length of value in bytes             */
    unsigned char   *value;      /* heap copy of raw value (may be NULL) */
    int64_t          ival;       /* decoded value for CFG_INT            */
    struct CfgEntry *next;
} CfgEntry;

typedef struct CfgProfile {
    CfgEntry *head;
    uint32_t  count;
} CfgProfile;

/* Status codes. */
#define CFG_OK         0
#define CFG_EBADMAGIC -1
#define CFG_EVERSION  -2
#define CFG_ETRUNC    -3
#define CFG_ELIMIT    -4
#define CFG_ENOMEM    -5
#define CFG_EIO       -6

/* --- parsing --- */
CFGCORE_API int  CfgParseBlob(const unsigned char *data, size_t len, CfgProfile **out);
CFGCORE_API int  CfgOpenFile(const wchar_t *path, CfgProfile **out);

/* --- construction (used by providers to build a profile) --- */
CFGCORE_API CfgProfile *CfgNew(void);
CFGCORE_API int         CfgAdd(CfgProfile *p, const char *name, int type,
                               const unsigned char *val, uint32_t len);

/* --- query / combine --- */
CFGCORE_API CfgEntry *CfgLookup(CfgProfile *p, const char *key);
CFGCORE_API int       CfgMerge(CfgProfile *base, const CfgProfile *over);
CFGCORE_API void      CfgFree(CfgProfile *p);

/* --- display helper (used by the CLI to pretty-print an entry) --- */
CFGCORE_API void CfgFormatLabel(const CfgEntry *e, char *out, size_t outsz);

/* --- serve-time helpers (used by cfgsvcd) --- */
/* Resolve ${KEY} references in a template entry against profile p, writing the
 * expanded ascii result to 'out'. Returns CFG_OK or a negative status. */
CFGCORE_API int  CfgExpandTemplate(CfgProfile *p, const CfgEntry *e, char *out);

/* Copy a blob value of 'declared_len' bytes from the wire into 'dst'. Used by
 * the push fast-path, which keeps a small inline scratch buffer per request. */
CFGCORE_API void CfgReadBlobInline(unsigned char *dst, const unsigned char *src,
                                   unsigned int declared_len);

#endif /* CASCADE_CFGCORE_H */
