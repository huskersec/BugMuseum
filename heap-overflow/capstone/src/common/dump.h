/* dump.h — Avalanche snapshot (dump) file format, read by cacheload.
 *
 * A snapshot is a header plus a sequence of entries. Each entry carries the
 * key, a short per-entry id tag (an opaque label used for bookkeeping), and the
 * value bytes.
 */
#ifndef AVALANCHE_DUMP_H
#define AVALANCHE_DUMP_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct avl_dump_header {
    char     magic[4];      /* 'A','V','L','1' */
    uint32_t entry_count;
} avl_dump_header;
/* each entry on disk:
 *   uint16_t key_len;   char    key[key_len];
 *   uint8_t  id_len;    uint8_t id[id_len];
 *   uint32_t value_len; uint8_t value[value_len];
 */
#pragma pack(pop)

#define AVL_DUMP_MAGIC0 'A'
#define AVL_DUMP_MAGIC1 'V'
#define AVL_DUMP_MAGIC2 'L'
#define AVL_DUMP_MAGIC3 '1'

#define AVL_DUMP_MAX_ENTRIES 65536u
#define AVL_DUMP_MAX_KEY     4096u
#define AVL_DUMP_MAX_VALUE   (1u << 20)

#endif /* AVALANCHE_DUMP_H */
