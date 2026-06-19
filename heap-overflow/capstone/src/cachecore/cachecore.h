/* cachecore.h — public API of cachecore.dll.
 *
 * The store: a hash table of key->value entries with a small-value inline
 * optimization, plus the serve-time helpers the server and loader use. Every
 * Avalanche frontend links it.
 */
#ifndef AVALANCHE_CACHECORE_H
#define AVALANCHE_CACHECORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef CACHECORE_EXPORTS
#define CACHECORE_API __declspec(dllexport)
#else
#define CACHECORE_API __declspec(dllimport)
#endif

typedef struct CacheEntry {
    char              *key;        /* NUL-terminated, heap                       */
    unsigned char     *value;      /* -> small[] for small values, else heap     */
    uint32_t           value_len;
    unsigned char      small[64];  /* inline buffer for small values             */
    struct CacheEntry *next;       /* bucket chain                               */
} CacheEntry;

typedef struct Cache {
    CacheEntry **buckets;
    uint32_t     nbuckets;
    uint32_t     count;
} Cache;

#define CACHE_OK         0
#define CACHE_ENOMEM    -1
#define CACHE_EIO       -2
#define CACHE_ENOTFOUND -3

/* lifecycle */
CACHECORE_API Cache *CacheNew(uint32_t nbuckets);
CACHECORE_API void   CacheFree(Cache *c);

/* operations */
CACHECORE_API int        CacheSet(Cache *c, const char *key, const unsigned char *val, uint32_t len);
CACHECORE_API CacheEntry *CacheGet(Cache *c, const char *key);     /* lenient (normalizes) — local pipe */
CACHECORE_API CacheEntry *CacheGetRaw(Cache *c, const char *key);  /* exact match — TCP                 */
CACHECORE_API int        CacheDel(Cache *c, const char *key);

/* fast-path store: create+link an empty entry, then stash a value inline */
CACHECORE_API CacheEntry *CacheInsertEmpty(Cache *c, const char *key);
CACHECORE_API void        CacheStoreFast(CacheEntry *e, const unsigned char *src, uint32_t declared_len);

/* stats helpers */
CACHECORE_API char *CacheFormatLine(const CacheEntry *e);  /* "key=... len=..." */
CACHECORE_API char *CachePreviewKey(const char *key);      /* short key preview */

/* snapshot helper (used by cacheload) */
CACHECORE_API char *CacheDecodeId(const unsigned char *src, uint32_t id_len);

#endif /* AVALANCHE_CACHECORE_H */
