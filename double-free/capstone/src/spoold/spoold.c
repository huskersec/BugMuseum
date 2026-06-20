/* spoold.exe — Encore spooler server.
 *
 * Serves the spool protocol over a named pipe (local) and a TCP socket
 * (loopback). Each connection can SUBMIT jobs and RENDER/APPEND/CANCEL/STATUS
 * them. Some ops are transport-specific: RECYCLE (buffer reuse) is a local pipe
 * convenience; DUP (reprint) and FLUSH (run the render pump) are server-side
 * network features. Ops are dispatched through a function-pointer table.
 * Loopback only — do not expose.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include "../spoolcore/spoolcore.h"
#include "../common/protocol.h"
#include "../common/filter.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

static SpoolTable      *g_table;
static CRITICAL_SECTION g_lock;

#define NFILTERS 3
static FilterFn         g_filters[NFILTERS];   /* resolved at startup via GetProcAddress */

typedef struct conn {
    int    is_tcp;
    SOCKET sock;
    HANDLE pipe;
} conn;

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

static FilterFn filter_for(uint8_t id) { return (id < NFILTERS) ? g_filters[id] : NULL; }

/* ---- op handlers (reached via the dispatch table) ------------------------ */

static void h_submit(conn *c, spool_request *r, const unsigned char *arg) {
    uint8_t fmt = r->fmt & FILTER_ID_MASK;
    FilterFn f = filter_for(fmt);                 /* text/consuming isn't offered on the wire */
    int id = JobSubmit(g_table, fmt, f, arg, r->arg_len);
    if (id < 0) { reply(c, "ERR submit\n"); return; }
    char msg[40];
    _snprintf(msg, sizeof msg, "OK job=%d\n", id);
    msg[sizeof msg - 1] = '\0';
    reply(c, msg);
}

static void h_render(conn *c, spool_request *r, const unsigned char *arg) {
    (void)arg;
    char out[256];
    int n = JobRender(g_table, (int)r->job, out, sizeof out);
    if (n < 0) { reply(c, "ERR\n"); return; }
    conn_write(c, out, n);
    reply(c, "\n");
}

static void h_append(conn *c, spool_request *r, const unsigned char *arg) {
    if (JobAppend(g_table, (int)r->job, arg, r->arg_len) != SPOOL_OK) { reply(c, "ERR\n"); return; }
    reply(c, "OK\n");
}

static void h_cancel(conn *c, spool_request *r, const unsigned char *arg) {
    (void)arg;
    JobCancel(g_table, (int)r->job);
    reply(c, "OK\n");
}

static void h_status(conn *c, spool_request *r, const unsigned char *arg) {
    (void)arg;
    int verbose = (r->fmt & FLAG_VERBOSE) && !c->is_tcp;   /* active preview is a local feature */
    char out[256];
    JobStatus(g_table, verbose, out, sizeof out);
    reply(c, out);
    reply(c, "\n");
}

static void h_recycle(conn *c, spool_request *r, const unsigned char *arg) {
    (void)arg;
    if (c->is_tcp) { reply(c, "ERR local-only\n"); return; }   /* buffer reuse is a pipe convenience */
    JobRecycle(g_table, (int)r->job);
    reply(c, "OK\n");
}

static void h_dup(conn *c, spool_request *r, const unsigned char *arg) {
    (void)arg;
    if (!c->is_tcp) { reply(c, "ERR net-only\n"); return; }    /* reprint is a server feature */
    int id = JobDup(g_table, (int)r->job);
    if (id < 0) { reply(c, "ERR dup\n"); return; }
    char msg[40];
    _snprintf(msg, sizeof msg, "OK job=%d\n", id);
    msg[sizeof msg - 1] = '\0';
    reply(c, msg);
}

static void h_flush(conn *c, spool_request *r, const unsigned char *arg) {
    (void)arg; (void)r;
    if (!c->is_tcp) { reply(c, "ERR net-only\n"); return; }    /* the render pump runs server-side */
    JobFlush(g_table);
    reply(c, "OK\n");
}

typedef void (*op_fn)(conn *, spool_request *, const unsigned char *);
static op_fn g_ops[8] = { h_submit, h_render, h_append, h_cancel, h_status, h_recycle, h_dup, h_flush };

/* ---- per-connection request loop ----------------------------------------- */

static void serve(conn *c) {
    for (;;) {
        spool_request req;
        if (conn_read_full(c, &req, sizeof req) != (int)sizeof req) break;
        if (req.arg_len > ENC_MAX_ARG) { reply(c, "ERR big\n"); break; }

        unsigned char *arg = NULL;
        if (req.arg_len) {
            arg = (unsigned char *)malloc(req.arg_len);
            if (!arg) break;
            if (conn_read_full(c, arg, (int)req.arg_len) != (int)req.arg_len) { free(arg); break; }
        }
        if (req.op > OP_FLUSH) { reply(c, "ERR op\n"); free(arg); continue; }

        EnterCriticalSection(&g_lock);
        g_ops[req.op](c, &req, arg);
        LeaveCriticalSection(&g_lock);

        free(arg);
    }
}

/* ---- filters ------------------------------------------------------------- */

static void load_filters(void) {
    /* The consuming text filter is not offered to network/pipe clients (index 1
     * is left unresolved); the wire formats are raster and pdfish. */
    static const wchar_t *names[NFILTERS] = { L"filter_raster.dll", NULL, L"filter_pdfish.dll" };
    for (int i = 0; i < NFILTERS; i++) {
        if (!names[i]) continue;
        HMODULE m = LoadLibraryExW(names[i], NULL,
                                   LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (m) g_filters[i] = (FilterFn)GetProcAddress(m, FILTER_ENTRY);
    }
}

/* ---- pipe server thread -------------------------------------------------- */

static unsigned __stdcall pipe_thread(void *arg) {
    (void)arg;
    for (;;) {
        HANDLE h = CreateNamedPipeW(SPOOL_PIPE_NAME, PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) break;
        if (ConnectNamedPipe(h, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            conn c; memset(&c, 0, sizeof c); c.is_tcp = 0; c.pipe = h;
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
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(SPOOL_TCP_PORT);

    BOOL yes = TRUE;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof yes);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) != 0) { closesocket(ls); return 1; }
    if (listen(ls, SOMAXCONN) != 0) { closesocket(ls); return 1; }

    printf("spoold: TCP 127.0.0.1:%d, pipe %ls\n", SPOOL_TCP_PORT, SPOOL_PIPE_NAME);
    fflush(stdout);

    for (;;) {
        SOCKET s = accept(ls, NULL, NULL);
        if (s == INVALID_SOCKET) continue;
        conn c; memset(&c, 0, sizeof c); c.is_tcp = 1; c.sock = s;
        serve(&c);
        closesocket(s);
    }
}

int main(void) {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 1;
    InitializeCriticalSection(&g_lock);
    g_table = SpoolNew();
    if (!g_table) { WSACleanup(); return 1; }
    load_filters();

    _beginthreadex(NULL, 0, pipe_thread, NULL, 0, NULL);
    int rc = tcp_loop();

    WSACleanup();
    return rc;
}
