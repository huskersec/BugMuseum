/* batch.h — Encore job-batch (.enc) file format, replayed by spoolc.
 *
 * A recorded sequence of spooler operations: a header, then `count` records,
 * each a wire-style request header followed by its payload bytes.
 */
#ifndef ENCORE_BATCH_H
#define ENCORE_BATCH_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct batch_header {
    char     magic[4];   /* 'E','N','C','1' */
    uint32_t count;
} batch_header;
/* each record: op u8 | fmt u8 | job u16 | arg_len u32 | arg[arg_len] */
#pragma pack(pop)

#define BATCH_MAGIC0 'E'
#define BATCH_MAGIC1 'N'
#define BATCH_MAGIC2 'C'
#define BATCH_MAGIC3 '1'

#define BATCH_MAX_RECORDS 65536u
#define BATCH_MAX_ARG     (1u << 20)

#endif /* ENCORE_BATCH_H */
