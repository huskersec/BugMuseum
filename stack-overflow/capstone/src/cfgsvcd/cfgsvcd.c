/* cfgsvcd.exe — Cascade configuration service.
 *
 * Exposes the same request protocol over two transports:
 *   - a named pipe  (\\.\pipe\cascade)  for local clients
 *   - a TCP socket  (127.0.0.1:7420)    for networked clients
 *
 * Ops are dispatched through a small handler table. The pipe serves keyed
 * GETs and PUSH; the TCP listener is push-only. The TCP listener binds
 * loopback only — this is intentionally-vulnerable research code and must not
 * be exposed on a real network.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include "../cfgcore/cfgcore.h"
#include "../common/protocol.h"
#include "../common/cprof.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

typedef struct conn {
    int    is_tcp;
    SOCKET sock;   /* valid when is_tcp  */
    HANDLE pipe;   /* valid when !is_tcp */
} conn;

static CfgProfile *g_store;
static CRITICAL_SECTION g_lock;   /* guards g_store across the pipe + TCP servers */

/* ---- transport-agnostic I/O ---------------------------------------------- */

static int conn_read(conn *c, void *buf, int len) {
    if (c->is_tcp) return recv(c->sock, (char *)buf, len, 0);
    DWORD got = 0;
    if (!ReadFile(c->pipe, buf, (DWORD)len, &got, NULL)) return -1;
    return (int)got;
}

static int conn_read_full(conn *c, void *buf, int len) {
    int off = 0;
    while (off < len) {
        int n = conn_read(c, (char *)buf + off, len - off);
        if (n <= 0) return -1;
        off += n;
    }
    return off;
}

static void conn_write(conn *c, const void *buf, int len) {
    if (c->is_tcp) { send(c->sock, (const char *)buf, len, 0); }
    else { DWORD wr; WriteFile(c->pipe, buf, (DWORD)len, &wr, NULL); }
}

static void reply(conn *c, const char *s) { conn_write(c, s, (int)strlen(s)); }

/* ---- helpers ------------------------------------------------------------- */

/* Find the first CFG_BLOB in a bundle; return its value pointer + declared
 * length. Confirms the declared bytes are present in the payload. */
static int peek_first_blob(const unsigned char *data, uint32_t len,
                           const unsigned char **blob, unsigned int *declared) {
    if (len < sizeof(cprof_header)) return 0;
    const cprof_header *h = (const cprof_header *)data;
    size_t off = sizeof(cprof_header);
    for (uint32_t i = 0; i < h->entry_count; i++) {
        if (off + 2 > len) return 0;
        uint16_t nl; memcpy(&nl, data + off, 2); off += 2;
        if (off + nl > len) return 0;
        off += nl;
        if (off + 1 > len) return 0;
        uint8_t type = data[off++];
        if (off + 4 > len) return 0;
        uint32_t vl; memcpy(&vl, data + off, 4); off += 4;
        if (off + vl > len) return 0;
        if (type == CFG_BLOB) { *blob = data + off; *declared = vl; return 1; }
        off += vl;
    }
    return 0;
}

static void materialize_templates(CfgProfile *p) {
    for (CfgEntry *e = p->head; e; e = e->next) {
        if (e->type == CFG_TEMPLATE) {
            char out[256];
            CfgExpandTemplate(p, e, out);
            /* (the expanded value would be cached/logged for the client here) */
        }
    }
}

/* ---- op handlers (reached via the dispatch table) ------------------------ */

static void handle_get(conn *c, cfg_request *r, unsigned char *payload) {
    char key[256];
    unsigned int kl = r->payload_len < 255 ? r->payload_len : 255;
    memcpy(key, payload, kl);
    key[kl] = '\0';

    CfgEntry *e = CfgLookup(g_store, key);
    if (!e) { reply(c, "ERR not found\n"); return; }

    if (e->type == CFG_TEMPLATE) {
        char out[256];
        CfgExpandTemplate(g_store, e, out);
        reply(c, out);
        reply(c, "\n");
    } else {
        reply(c, "OK\n");
    }
}

static void handle_push(conn *c, cfg_request *r, unsigned char *payload) {
    if (c->is_tcp) {
        /* network fast-path: copy the first blob into a scratch buffer for a
         * quick signature check before the full ingest below. */
        const unsigned char *blob;
        unsigned int declared;
        if (peek_first_blob(payload, r->payload_len, &blob, &declared)) {
            unsigned char scratch[64];
            CfgReadBlobInline(scratch, blob, declared);
            (void)scratch;
        }
    }

    CfgProfile *p = NULL;
    if (CfgParseBlob(payload, r->payload_len, &p) == CFG_OK) {
        materialize_templates(p);
        CfgMerge(g_store, p);
        CfgFree(p);
        reply(c, "OK\n");
    } else {
        reply(c, "ERR parse\n");
    }
}

typedef void (*op_handler)(conn *, cfg_request *, unsigned char *);
static op_handler g_handlers[2] = { handle_get, handle_push };

/* ---- request loop -------------------------------------------------------- */

static void serve(conn *c) {
    cfg_request req;
    if (conn_read_full(c, &req, sizeof(req)) != (int)sizeof(req)) return;
    if (req.payload_len > CASCADE_MAX_PAYLOAD) { reply(c, "ERR too big\n"); return; }

    unsigned char *payload = (unsigned char *)malloc(req.payload_len ? req.payload_len : 1);
    if (!payload) return;
    if (req.payload_len &&
        conn_read_full(c, payload, (int)req.payload_len) != (int)req.payload_len) {
        free(payload);
        return;
    }

    if (req.op > OP_PUSH) { reply(c, "ERR op\n"); free(payload); return; }
    /* Transport policy: the keyed GET is local (pipe) only; TCP is push-only. */
    if (c->is_tcp && req.op == OP_GET) { reply(c, "ERR unsupported\n"); free(payload); return; }

    EnterCriticalSection(&g_lock);
    g_handlers[req.op](c, &req, payload);
    LeaveCriticalSection(&g_lock);
    free(payload);
}

/* ---- pipe server thread -------------------------------------------------- */

static unsigned __stdcall pipe_thread(void *arg) {
    (void)arg;
    for (;;) {
        HANDLE h = CreateNamedPipeW(CASCADE_PIPE_NAME, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) break;
        if (ConnectNamedPipe(h, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            conn c; c.is_tcp = 0; c.pipe = h;
            serve(&c);
            FlushFileBuffers(h);
        }
        DisconnectNamedPipe(h);
        CloseHandle(h);
    }
    return 0;
}

/* ---- tcp accept loop ----------------------------------------------------- */

static int tcp_loop(void) {
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) return 1;

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(CASCADE_TCP_PORT);

    BOOL yes = TRUE;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
    if (bind(ls, (struct sockaddr *)&a, sizeof(a)) != 0) { closesocket(ls); return 1; }
    if (listen(ls, SOMAXCONN) != 0) { closesocket(ls); return 1; }

    printf("cfgsvcd: TCP 127.0.0.1:%d, pipe %ls\n", CASCADE_TCP_PORT, CASCADE_PIPE_NAME);
    fflush(stdout);

    for (;;) {
        SOCKET s = accept(ls, NULL, NULL);
        if (s == INVALID_SOCKET) continue;
        conn c; c.is_tcp = 1; c.sock = s;
        serve(&c);
        closesocket(s);
    }
}

int main(void) {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 1;
    InitializeCriticalSection(&g_lock);
    g_store = CfgNew();
    if (!g_store) { WSACleanup(); return 1; }

    _beginthreadex(NULL, 0, pipe_thread, NULL, 0, NULL);
    int rc = tcp_loop();

    WSACleanup();
    return rc;
}
