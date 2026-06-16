/* cprof.h — on-disk layout of a Cascade ".cprof" profile bundle.
 *
 * A bundle is a small header followed by a sequence of length-prefixed
 * entries and a trailing CRC. Entries are NOT fixed-size structs on disk
 * (names and values are variable length), so the reader walks the buffer
 * field by field. See cfgcore for the in-memory model.
 */
#ifndef CASCADE_CPROF_H
#define CASCADE_CPROF_H

#include <stdint.h>

#define CPROF_MAGIC0 'C'
#define CPROF_MAGIC1 'P'
#define CPROF_MAGIC2 'R'
#define CPROF_MAGIC3 'F'
#define CPROF_VERSION 1

/* Entry value kinds. */
enum cprof_type {
    CFG_STR      = 0,  /* ascii string      */
    CFG_WSTR     = 1,  /* utf-16 string     */
    CFG_BLOB     = 2,  /* opaque bytes      */
    CFG_INT      = 3,  /* decimal integer, stored as ascii digits */
    CFG_TEMPLATE = 4   /* ascii string with ${KEY} references     */
};

#pragma pack(push, 1)
typedef struct cprof_header {
    char     magic[4];     /* 'C','P','R','F'            */
    uint16_t version;      /* CPROF_VERSION              */
    uint16_t flags;        /* reserved                   */
    uint32_t entry_count;  /* number of entries to read  */
} cprof_header;
/* Each entry on disk:
 *   uint16_t name_len;  char    name[name_len];
 *   uint8_t  type;       uint32_t value_len;  uint8_t value[value_len];
 * Followed once, after all entries:
 *   uint32_t crc32;
 */
#pragma pack(pop)

/* Sanity ceilings the reader enforces before trusting counts/lengths. */
#define CPROF_MAX_ENTRIES   4096u
#define CPROF_MAX_NAME      1024u
#define CPROF_MAX_VALUE     (1u << 20)   /* 1 MiB */

#endif /* CASCADE_CPROF_H */
