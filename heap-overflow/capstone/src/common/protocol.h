/* protocol.h — Avalanche cache wire protocol (shared by cached and clients).
 *
 * cached exposes the same request shape over two transports: a named pipe
 * (local) and a TCP socket (loopback). A request is a fixed header followed by
 * the key bytes and then the value bytes.
 */
#ifndef AVALANCHE_PROTOCOL_H
#define AVALANCHE_PROTOCOL_H

#include <stdint.h>

enum cache_op {
    OP_SET   = 0,
    OP_GET   = 1,
    OP_DEL   = 2,
    OP_STATS = 3
};

#define FLAG_FAST        0x01u   /* bit0: inline fast-path store (TCP) */
#define FLAG_VERBOSE     0x02u   /* bit1: verbose STATS (includes key previews) */
#define FLAG_CODEC_MASK  0xF0u   /* bits4..7: value codec id */
#define FLAG_CODEC_SHIFT 4

#pragma pack(push, 1)
typedef struct cache_request {
    uint8_t  op;          /* enum cache_op           */
    uint8_t  flags;       /* FLAG_* bits             */
    uint16_t key_len;     /* bytes of key follow     */
    uint32_t value_len;   /* bytes of value follow   */
} cache_request;
#pragma pack(pop)

#define AVL_TCP_PORT    7000
#define AVL_PIPE_NAME   L"\\\\.\\pipe\\avalanche"
#define AVL_MAX_PAYLOAD (1u << 20)   /* 1 MiB cap on key+value per request */

#endif /* AVALANCHE_PROTOCOL_H */
