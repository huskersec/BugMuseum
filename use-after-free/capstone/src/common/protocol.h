/* protocol.h — Revenant session wire protocol (shared by sessiond and clients).
 *
 * sessiond exposes the same request shape over a named pipe (local) and a TCP
 * socket (loopback). A request is a fixed header followed by an argument blob.
 * Sessions are referenced by the numeric `handle` returned from OPEN.
 */
#ifndef REVENANT_PROTOCOL_H
#define REVENANT_PROTOCOL_H

#include <stdint.h>

enum sess_op {
    OP_OPEN       = 0,   /* create a session of `type`; reply carries the handle */
    OP_USE_RUN    = 1,   /* invoke the session's handler on arg                  */
    OP_USE_SET    = 2,   /* overwrite the session's data with arg                */
    OP_USE_APPEND = 3,   /* append arg to the session's data                     */
    OP_CLOSE      = 4,   /* destroy the session                                  */
    OP_STAT       = 5,   /* summary (verbose flag adds a recent-session preview)  */
    OP_FLUSH      = 6    /* run the deferred batch                               */
};

/* `type` byte: at OPEN it selects the handler (0=echo, 1=calc); for the USE and
 * STAT ops its high bits carry flags. */
#define HANDLER_ID_MASK 0x0Fu
#define FLAG_DEFER      0x80u   /* USE_*: also record this session in the deferred batch */
#define FLAG_VERBOSE    0x40u   /* STAT: include the most-recent-session preview          */

#pragma pack(push, 1)
typedef struct sess_request {
    uint8_t  op;          /* enum sess_op            */
    uint8_t  type;        /* handler id / flags      */
    uint16_t handle;      /* session handle id       */
    uint32_t arg_len;     /* bytes of arg follow     */
} sess_request;
#pragma pack(pop)

#define SESS_TCP_PORT   7100
#define SESS_PIPE_NAME  L"\\\\.\\pipe\\revenant"
#define SESS_MAX_ARG    (1u << 20)

#endif /* REVENANT_PROTOCOL_H */
