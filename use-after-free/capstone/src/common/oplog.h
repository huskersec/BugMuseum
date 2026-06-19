/* oplog.h — Revenant op-log (.rvn) file format, replayed by sessreplay.
 *
 * A recorded sequence of session operations: a header, then `count` records,
 * each a wire-style request header followed by its argument bytes.
 */
#ifndef REVENANT_OPLOG_H
#define REVENANT_OPLOG_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct oplog_header {
    char     magic[4];   /* 'R','V','N','1' */
    uint32_t count;
} oplog_header;
/* each record: op u8 | type u8 | handle u16 | arg_len u32 | arg[arg_len] */
#pragma pack(pop)

#define OPLOG_MAGIC0 'R'
#define OPLOG_MAGIC1 'V'
#define OPLOG_MAGIC2 'N'
#define OPLOG_MAGIC3 '1'

#define OPLOG_MAX_RECORDS 65536u
#define OPLOG_MAX_ARG     (1u << 20)

#endif /* REVENANT_OPLOG_H */
