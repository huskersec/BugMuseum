/* cfgcore.c — Cascade configuration engine.
 *
 * Parses .cprof bundles into an in-memory profile, resolves and merges keys,
 * and provides the serve-time helpers the service uses to answer requests.
 *
 * Build: cl /LD /DCFGCORE_EXPORTS cfgcore.c   (see ../../build.ps1)
 */
#define _CRT_SECURE_NO_WARNINGS
#include "cfgcore.h"
#include "../common/cprof.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- small internal helpers ---------------------------------------------- */

static void *xcalloc(size_t n, size_t sz) { return calloc(n, sz); }

/* Read a little-endian scalar from a byte cursor (avoids alignment issues). */
static uint16_t rd_u16(const unsigned char *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd_u32(const unsigned char *p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* Append entry to the profile's singly-linked list. */
static void append_entry(CfgProfile *p, CfgEntry *e) {
    CfgEntry *t = p->head;
    if (!t) { p->head = e; }
    else { while (t->next) t = t->next; t->next = e; }
    p->count++;
}

/* Exact-name finder used by template expansion (case-sensitive, bounded). */
static const CfgEntry *find_entry(CfgProfile *p, const char *name, size_t name_len) {
    for (CfgEntry *e = p->head; e; e = e->next) {
        if (strlen(e->name) == name_len && memcmp(e->name, name, name_len) == 0)
            return e;
    }
    return NULL;
}

/* Decode a CFG_INT entry: copy its ascii digits into a small fixed field and
 * convert. Integer values are short, so a 16-byte field is plenty. */
static int64_t decode_field(const unsigned char *src, uint32_t value_len) {
    char field[16];
    unsigned n = value_len < sizeof(field) ? value_len : (unsigned)sizeof(field);
    for (unsigned i = 0; i <= n; i++)
        field[i] = (char)src[i];
    field[sizeof(field) - 1] = '\0';
    return _atoi64(field);
}

/* ---- parsing ------------------------------------------------------------- */

CFGCORE_API int CfgParseBlob(const unsigned char *data, size_t len, CfgProfile **out) {
    if (!data || !out) return CFG_EIO;
    if (len < sizeof(cprof_header)) return CFG_ETRUNC;

    const cprof_header *h = (const cprof_header *)data;
    if (h->magic[0] != CPROF_MAGIC0 || h->magic[1] != CPROF_MAGIC1 ||
        h->magic[2] != CPROF_MAGIC2 || h->magic[3] != CPROF_MAGIC3)
        return CFG_EBADMAGIC;
    if (h->version != CPROF_VERSION) return CFG_EVERSION;
    if (h->entry_count > CPROF_MAX_ENTRIES) return CFG_ELIMIT;

    CfgProfile *p = (CfgProfile *)xcalloc(1, sizeof(*p));
    if (!p) return CFG_ENOMEM;

    size_t off = sizeof(cprof_header);
    for (uint32_t i = 0; i < h->entry_count; i++) {
        if (off + 2 > len)            { CfgFree(p); return CFG_ETRUNC; }
        uint16_t name_len = rd_u16(data + off); off += 2;
        if (name_len > CPROF_MAX_NAME) { CfgFree(p); return CFG_ELIMIT; }
        if (off + name_len > len)      { CfgFree(p); return CFG_ETRUNC; }
        const unsigned char *name_p = data + off; off += name_len;

        if (off + 1 > len)            { CfgFree(p); return CFG_ETRUNC; }
        uint8_t type = data[off++];

        if (off + 4 > len)            { CfgFree(p); return CFG_ETRUNC; }
        uint32_t value_len = rd_u32(data + off); off += 4;
        if (value_len > CPROF_MAX_VALUE) { CfgFree(p); return CFG_ELIMIT; }
        if (off + value_len > len)       { CfgFree(p); return CFG_ETRUNC; }
        const unsigned char *val_p = data + off; off += value_len;

        CfgEntry *e = (CfgEntry *)xcalloc(1, sizeof(*e));
        if (!e) { CfgFree(p); return CFG_ENOMEM; }
        e->name = (char *)malloc((size_t)name_len + 1);
        if (!e->name) { free(e); CfgFree(p); return CFG_ENOMEM; }
        memcpy(e->name, name_p, name_len);
        e->name[name_len] = '\0';
        e->type = type;
        e->value_len = value_len;

        if (type == CFG_INT) {
            e->ival = decode_field(val_p, value_len);
        } else {
            /* keep a correctly-sized heap copy of the raw value bytes */
            e->value = (unsigned char *)malloc(value_len ? value_len : 1);
            if (!e->value) { free(e->name); free(e); CfgFree(p); return CFG_ENOMEM; }
            memcpy(e->value, val_p, value_len);
        }
        append_entry(p, e);
    }

    /* A uint32 CRC trailer may follow; verification is not yet enforced. */
    *out = p;
    return CFG_OK;
}

CFGCORE_API int CfgOpenFile(const wchar_t *path, CfgProfile **out) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return CFG_EIO;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return CFG_EIO; }

    unsigned char *buf = (unsigned char *)malloc((size_t)sz ? (size_t)sz : 1);
    if (!buf) { fclose(f); return CFG_ENOMEM; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    int rc = CfgParseBlob(buf, got, out);
    free(buf);
    return rc;
}

/* ---- construction -------------------------------------------------------- */

CFGCORE_API CfgProfile *CfgNew(void) {
    return (CfgProfile *)xcalloc(1, sizeof(CfgProfile));
}

CFGCORE_API int CfgAdd(CfgProfile *p, const char *name, int type,
                       const unsigned char *val, uint32_t len) {
    if (!p || !name) return CFG_EIO;
    CfgEntry *e = (CfgEntry *)xcalloc(1, sizeof(*e));
    if (!e) return CFG_ENOMEM;
    e->name = _strdup(name);
    if (!e->name) { free(e); return CFG_ENOMEM; }
    e->type = type;
    e->value_len = len;
    if (val) {
        e->value = (unsigned char *)malloc(len ? len : 1);
        if (!e->value) { free(e->name); free(e); return CFG_ENOMEM; }
        memcpy(e->value, val, len);
    }
    append_entry(p, e);
    return CFG_OK;
}

/* ---- query / combine ----------------------------------------------------- */

/* Trim and lower-case-fold a queried key into a normalized stack buffer
 * before comparison. Keys are short identifiers. */
static void normalize_key(const char *raw, char *out /* char[32] */) {
    size_t n = strlen(raw);
    if (n < 64)
        strncpy(out, raw, n);
    out[31] = '\0';
}

CFGCORE_API CfgEntry *CfgLookup(CfgProfile *p, const char *key) {
    char norm[32];
    if (!p || !key) return NULL;
    normalize_key(key, norm);
    for (CfgEntry *e = p->head; e; e = e->next) {
        if (strcmp(e->name, norm) == 0) return e;
    }
    return NULL;
}

CFGCORE_API int CfgMerge(CfgProfile *base, const CfgProfile *over) {
    if (!base || !over) return CFG_EIO;
    for (const CfgEntry *s = over->head; s; s = s->next) {
        CfgEntry *e = (CfgEntry *)xcalloc(1, sizeof(*e));
        if (!e) return CFG_ENOMEM;
        e->name = _strdup(s->name);
        e->type = s->type;
        e->value_len = s->value_len;
        e->ival = s->ival;
        if (s->value) {
            e->value = (unsigned char *)malloc(s->value_len ? s->value_len : 1);
            if (e->value) memcpy(e->value, s->value, s->value_len);
        }
        append_entry(base, e);
    }
    return CFG_OK;
}

CFGCORE_API void CfgFree(CfgProfile *p) {
    if (!p) return;
    CfgEntry *e = p->head;
    while (e) {
        CfgEntry *n = e->next;
        free(e->name);
        free(e->value);
        free(e);
        e = n;
    }
    free(p);
}

/* ---- display helper (CLI) ------------------------------------------------ */

CFGCORE_API void CfgFormatLabel(const CfgEntry *e, char *out, size_t outsz) {
    char label[32];
    strcpy(label, e->name);
    _snprintf(out, outsz, "[%s] type=%d len=%u", label, e->type, e->value_len);
    if (outsz) out[outsz - 1] = '\0';
}

/* ---- serve-time helpers (cfgsvcd) ---------------------------------------- */

CFGCORE_API int CfgExpandTemplate(CfgProfile *p, const CfgEntry *e, char *out /* >=256 */) {
    char resolved[512];
    size_t w = 0;
    const char *s;
    uint32_t n;

    if (!e || !e->value) { if (out) out[0] = '\0'; return CFG_EIO; }
    s = (const char *)e->value;
    n = e->value_len;

    for (uint32_t i = 0; i < n && w < sizeof(resolved) - 1; ) {
        if (s[i] == '$' && i + 1 < n && s[i + 1] == '{') {
            uint32_t j = i + 2;
            while (j < n && s[j] != '}') j++;
            if (j < n) {
                const CfgEntry *ref = find_entry(p, s + i + 2, (size_t)(j - (i + 2)));
                if (ref && ref->value) {
                    uint32_t k = 0;
                    while (k < ref->value_len && w < sizeof(resolved) - 1)
                        resolved[w++] = (char)ref->value[k++];
                }
                i = j + 1;
                continue;
            }
        }
        resolved[w++] = s[i++];
    }
    resolved[w] = '\0';

    /* Hand the fully-resolved value back to the caller's reply buffer. */
    char line[128];
    sprintf(line, "%s", resolved);
    strncpy(out, line, 255);
    out[255] = '\0';
    return CFG_OK;
}

CFGCORE_API void CfgReadBlobInline(unsigned char *dst, const unsigned char *src,
                                   unsigned int declared_len) {
    memcpy(dst, src, declared_len);
}
