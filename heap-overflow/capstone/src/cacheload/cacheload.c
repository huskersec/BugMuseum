/* cacheload.exe — local CLI: load a snapshot (.avl) file into a fresh store.
 *
 * Usage: cacheload <snapshot.avl>
 *
 * The local-file frontend: untrusted input is the snapshot named on the command
 * line. It parses the dump format and replays each entry into the cache.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "../cachecore/cachecore.h"
#include "../common/dump.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rd(FILE *f, void *buf, size_t n) { return fread(buf, 1, n, f) == n; }

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) { fwprintf(stderr, L"usage: cacheload <snapshot.avl>\n"); return 2; }

    FILE *f = _wfopen(argv[1], L"rb");
    if (!f) { fwprintf(stderr, L"cacheload: cannot open %s\n", argv[1]); return 1; }

    avl_dump_header h;
    if (!rd(f, &h, sizeof h) ||
        h.magic[0] != AVL_DUMP_MAGIC0 || h.magic[1] != AVL_DUMP_MAGIC1 ||
        h.magic[2] != AVL_DUMP_MAGIC2 || h.magic[3] != AVL_DUMP_MAGIC3) {
        fwprintf(stderr, L"cacheload: bad snapshot\n"); fclose(f); return 1;
    }
    if (h.entry_count > AVL_DUMP_MAX_ENTRIES) { fclose(f); return 1; }

    Cache *cache = CacheNew(1024);
    if (!cache) { fclose(f); return 1; }

    uint32_t loaded = 0;
    for (uint32_t i = 0; i < h.entry_count; i++) {
        uint16_t key_len;
        if (!rd(f, &key_len, 2) || key_len > AVL_DUMP_MAX_KEY) break;
        char *key = (char *)malloc((size_t)key_len + 1);
        if (!key) break;
        if (!rd(f, key, key_len)) { free(key); break; }
        key[key_len] = '\0';

        uint8_t id_len;
        if (!rd(f, &id_len, 1)) { free(key); break; }
        unsigned char *id = (unsigned char *)malloc(id_len ? id_len : 1);
        if (!id) { free(key); break; }
        if (id_len && !rd(f, id, id_len)) { free(key); free(id); break; }
        char *decoded = CacheDecodeId(id, id_len);   /* per-entry id tag */
        free(id);

        uint32_t value_len;
        if (!rd(f, &value_len, 4) || value_len > AVL_DUMP_MAX_VALUE) { free(key); free(decoded); break; }
        unsigned char *value = (unsigned char *)malloc(value_len ? value_len : 1);
        if (!value) { free(key); free(decoded); break; }
        if (value_len && !rd(f, value, value_len)) { free(key); free(decoded); free(value); break; }

        CacheSet(cache, key, value, value_len);

        free(key); free(decoded); free(value);
        loaded++;
    }

    fclose(f);
    printf("loaded %u entrie(s)\n", loaded);
    CacheFree(cache);
    return 0;
}
