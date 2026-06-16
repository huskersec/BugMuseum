/* protocol.h — Cascade service wire protocol (shared by cfgsvcd and clients).
 *
 * cfgsvcd exposes the same request shape over two transports: a named pipe
 * (local) and a TCP socket (remote). A request is a small fixed header
 * followed by a payload whose meaning depends on the op:
 *
 *   OP_GET   payload = ascii key name to resolve and return
 *   OP_PUSH  payload = a .cprof bundle to ingest into the live store
 */
#ifndef CASCADE_PROTOCOL_H
#define CASCADE_PROTOCOL_H

#include <stdint.h>

enum cfg_op {
    OP_GET  = 0,
    OP_PUSH = 1
};

enum cfg_source {
    SRC_FILE = 0,
    SRC_REG  = 1,
    SRC_ENV  = 2
};

#pragma pack(push, 1)
typedef struct cfg_request {
    uint8_t  op;           /* enum cfg_op            */
    uint8_t  source;       /* enum cfg_source        */
    uint16_t reserved;     /* must be 0              */
    uint32_t payload_len;  /* bytes of payload follow */
} cfg_request;
#pragma pack(pop)

#define CASCADE_TCP_PORT   7420
#define CASCADE_PIPE_NAME  L"\\\\.\\pipe\\cascade"

/* Largest request payload the service will read off the wire. */
#define CASCADE_MAX_PAYLOAD (1u << 20)   /* 1 MiB */

#endif /* CASCADE_PROTOCOL_H */
