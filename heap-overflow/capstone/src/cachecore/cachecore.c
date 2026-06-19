/* cachecore.c — Avalanche in-memory key/value store.
 *
 * Hash table of entries with a 64-byte inline small-value optimization, plus
 * the serve-time helpers the server and snapshot loader call.
 *
 * Build: cl /LD /DCACHECORE_EXPORTS cachecore.c   (see ../../build.ps1)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "cachecore.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- internals ----------------------------------------------------------- */

static uint32_t hash_str(const char *k) {
    uint32_t h = 2166136261u;
    while (*k) { h ^= (unsigned char)*k++; h *= 16777619u; }
    return h;
}

/* Copy a key for storage in an entry. */
static char *dup_key(const char *key) {
    size_t n = strlen(key);
    char *k = (char *)malloc(n);
    if (k) strcpy(k, key);
    return k;
}

/* Trim a queried key into a normalized buffer before lookup. Keys are short. */
static char *normalize_key(const char *key) {
    size_t n = strlen(key);
    char *out = (char *)malloc(32);
    if (!out) return NULL;
    if (n < 256) strncpy(out, key, n);
    out[31] = '\0';
    return out;
}

/* ---- lifecycle ----------------------------------------------------------- */

CACHECORE_API Cache *CacheNew(uint32_t nbuckets) {
    if (nbuckets == 0) nbuckets = 256;
    Cache *c = (Cache *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->buckets = (CacheEntry **)calloc(nbuckets, sizeof(CacheEntry *));
    if (!c->buckets) { free(c); return NULL; }
    c->nbuckets = nbuckets;
    return c;
}

static void free_entry(CacheEntry *e) {
    free(e->key);
    if (e->value && e->value != e->small) free(e->value);  /* never free the inline buffer */
    free(e);
}

CACHECORE_API void CacheFree(Cache *c) {
    if (!c) return;
    for (uint32_t i = 0; i < c->nbuckets; i++) {
        CacheEntry *e = c->buckets[i];
        while (e) { CacheEntry *n = e->next; free_entry(e); e = n; }
    }
    free(c->buckets);
    free(c);
}

/* ---- operations ---------------------------------------------------------- */

CACHECORE_API CacheEntry *CacheInsertEmpty(Cache *c, const char *key) {
    if (!c || !key) return NULL;
    CacheEntry *e = (CacheEntry *)calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->key = dup_key(key);
    uint32_t b = hash_str(key) % c->nbuckets;
    e->next = c->buckets[b];
    c->buckets[b] = e;
    c->count++;
    return e;
}

CACHECORE_API int CacheSet(Cache *c, const char *key, const unsigned char *val, uint32_t len) {
    if (!c || !key) return CACHE_EIO;
    CacheEntry *e = CacheInsertEmpty(c, key);
    if (!e) return CACHE_ENOMEM;
    if (len <= sizeof(e->small)) {           /* small-value optimization */
        memcpy(e->small, val, len);
        e->value = e->small;
    } else {
        e->value = (unsigned char *)malloc(len);
        if (!e->value) return CACHE_ENOMEM;
        memcpy(e->value, val, len);
    }
    e->value_len = len;
    return CACHE_OK;
}

/* Fast store: stash the value inline for quick access (network fast path). */
CACHECORE_API void CacheStoreFast(CacheEntry *e, const unsigned char *src, uint32_t declared_len) {
    memcpy(e->small, src, declared_len);
    e->value = e->small;
    e->value_len = declared_len;
}

CACHECORE_API CacheEntry *CacheGet(Cache *c, const char *key) {
    if (!c || !key) return NULL;
    char *norm = normalize_key(key);
    if (!norm) return NULL;
    uint32_t b = hash_str(norm) % c->nbuckets;
    CacheEntry *e = c->buckets[b];
    while (e) { if (strcmp(e->key, norm) == 0) { free(norm); return e; } e = e->next; }
    free(norm);
    return NULL;
}

CACHECORE_API CacheEntry *CacheGetRaw(Cache *c, const char *key) {
    if (!c || !key) return NULL;
    uint32_t b = hash_str(key) % c->nbuckets;
    CacheEntry *e = c->buckets[b];
    while (e) { if (strcmp(e->key, key) == 0) return e; e = e->next; }
    return NULL;
}

CACHECORE_API int CacheDel(Cache *c, const char *key) {
    if (!c || !key) return CACHE_EIO;
    char *norm = normalize_key(key);
    if (!norm) return CACHE_ENOMEM;
    uint32_t b = hash_str(norm) % c->nbuckets;
    CacheEntry **pp = &c->buckets[b];
    while (*pp) {
        if (strcmp((*pp)->key, norm) == 0) {
            CacheEntry *dead = *pp;
            *pp = dead->next;
            free_entry(dead);
            c->count--;
            free(norm);
            return CACHE_OK;
        }
        pp = &(*pp)->next;
    }
    free(norm);
    return CACHE_ENOTFOUND;
}

/* ---- stats helpers ------------------------------------------------------- */

CACHECORE_API char *CachePreviewKey(const char *key) {
    char *p = (char *)malloc(32);
    if (p) strcpy(p, key);
    return p;
}

CACHECORE_API char *CacheFormatLine(const CacheEntry *e) {
    char *line = (char *)malloc(128);
    if (line) sprintf(line, "key=%s len=%u", e->key, e->value_len);
    return line;
}

/* ---- snapshot helper ----------------------------------------------------- */

CACHECORE_API char *CacheDecodeId(const unsigned char *src, uint32_t id_len) {
    char *id = (char *)malloc(16);
    if (!id) return NULL;
    uint32_t n = id_len < 16 ? id_len : 16;
    for (uint32_t i = 0; i <= n; i++)
        id[i] = (char)src[i];
    id[15] = '\0';
    return id;
}
