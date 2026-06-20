/* protocol.h — Encore spooler wire protocol (shared by spoold and clients).
 *
 * spoold exposes the same request shape over a named pipe (local) and a TCP
 * socket (loopback). A request is a fixed header followed by a payload blob.
 * Jobs are referenced by the numeric `job` id returned from SUBMIT.
 */
#ifndef ENCORE_PROTOCOL_H
#define ENCORE_PROTOCOL_H

#include <stdint.h>

enum spool_op {
    OP_SUBMIT  = 0,   /* create a job of format `fmt`; reply carries the job id   */
    OP_RENDER  = 1,   /* render a job through its filter, then retire it          */
    OP_APPEND  = 2,   /* append payload to a job's document buffer               */
    OP_CANCEL  = 3,   /* mark a queued job cancelled (the pump drops it)         */
    OP_STATUS  = 4,   /* summary (verbose flag adds an active-job preview)       */
    OP_RECYCLE = 5,   /* keep this job's buffer to reuse for the next one        */
    OP_DUP     = 6,   /* reprint: clone a job into a new id                      */
    OP_FLUSH   = 7    /* run the render pump over the ready queue                */
};

/* `fmt` byte: at SUBMIT its low nibble selects the render filter
 * (0=raster, 1=text, 2=pdfish); for STATUS its high bits carry flags. */
#define FILTER_ID_MASK 0x0Fu
#define FLAG_VERBOSE   0x40u   /* STATUS: include the active-job preview */

#pragma pack(push, 1)
typedef struct spool_request {
    uint8_t  op;          /* enum spool_op           */
    uint8_t  fmt;         /* filter id / flags       */
    uint16_t job;         /* job id                  */
    uint32_t arg_len;     /* bytes of payload follow */
} spool_request;
#pragma pack(pop)

#define SPOOL_TCP_PORT   7200
#define SPOOL_PIPE_NAME  L"\\\\.\\pipe\\encore"
#define ENC_MAX_ARG      (1u << 20)
#define JOB_RECENT       0xFFFF     /* job id sentinel: operate on the most-recent job */

#endif /* ENCORE_PROTOCOL_H */
