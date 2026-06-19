/* sessiond.exe — Revenant session server.
 *
 * Serves the session protocol over a named pipe (local) and a TCP socket
 * (loopback). Each connection can OPEN/USE/CLOSE/STAT sessions. Handle
 * SESS_RECENT means "the most-recent session": on TCP that's the store's global
 * recent; on the pipe it's this connection's cached current pointer. Ops are
 * dispatched through a function-pointer table. Loopback only — do not expose.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include "../sesscore/sesscore.h"
#include "../common/protocol.h"
#include "../common/handler.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

static SessTable       *g_table;
static CRITICAL_SECTION g_lock;

#define NPLUGINS 16
static SessHandlerFn    g_plugins[NPLUGINS];   /* resolved at startup via GetProcAddress */

typedef struct conn {
    int     is_tcp;
    SOCKET  sock;
    HANDLE  pipe;
    Session *cur;     /* this connection's cached current session (pipe fast path) */
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

static SessHandlerFn plugin_for(uint8_t id) { return (id < NPLUGINS) ? g_plugins[id] : NULL; }

/* ---- op handlers (reached via the dispatch table) ------------------------ */

static void h_open(conn *c, sess_request *r, const unsigned char *arg) {
    uint8_t type = r->type & HANDLER_ID_MASK;
    SessHandlerFn h = plugin_for(type);
    int id = SessOpen(g_table, type, h, arg, r->arg_len);
    if (id < 0) { reply(c, "ERR open\n"); return; }
    if (!c->is_tcp) c->cur = SessGet(g_table, id);   /* pipe: remember the current session */
    char msg[40];
    _snprintf(msg, sizeof msg, "OK handle=%d\n", id);
    msg[sizeof msg - 1] = '\0';
    reply(c, msg);
}

static void h_use_run(conn *c, sess_request *r, const unsigned char *arg) {
    char out[256];
    int n;
    if (!c->is_tcp && r->handle == SESS_RECENT) {
        if (!c->cur) { reply(c, "ERR no current\n"); return; }
        n = c->cur->handler(arg, r->arg_len, out, sizeof out);   /* pipe: call via cached cur */
    } else {
        n = SessUseRun(g_table, (int)r->handle, arg, r->arg_len, out, sizeof out);
    }
    if (n < 0) { reply(c, "ERR\n"); return; }
    conn_write(c, out, n);
    reply(c, "\n");
    if (r->type & FLAG_DEFER) SessDefer(g_table, (int)r->handle);
}

static void h_use_set(conn *c, sess_request *r, const unsigned char *arg) {
    if (!c->is_tcp && r->handle == SESS_RECENT) {
        if (!c->cur) { reply(c, "ERR no current\n"); return; }
        uint32_t m = r->arg_len < c->cur->cap ? r->arg_len : c->cur->cap;   /* pipe: write via cached cur */
        if (m) memcpy(c->cur->data, arg, m);
        c->cur->len = m;
    } else {
        if (SessUseSet(g_table, (int)r->handle, arg, r->arg_len) != SESS_OK) { reply(c, "ERR\n"); return; }
    }
    if (r->type & FLAG_DEFER) SessDefer(g_table, (int)r->handle);
    reply(c, "OK\n");
}

static void h_use_append(conn *c, sess_request *r, const unsigned char *arg) {
    if (SessUseAppend(g_table, (int)r->handle, arg, r->arg_len) != SESS_OK) { reply(c, "ERR\n"); return; }
    reply(c, "OK\n");
}

static void h_close(conn *c, sess_request *r, const unsigned char *arg) {
    (void)arg;
    SessClose(g_table, (int)r->handle);   /* frees + clears the handle; c->cur is NOT updated */
    reply(c, "OK\n");
}

static void h_stat(conn *c, sess_request *r, const unsigned char *arg) {
    (void)arg;
    int verbose = (r->type & FLAG_VERBOSE) && !c->is_tcp;   /* recent preview is a local feature */
    char out[256];
    SessStat(g_table, verbose, out, sizeof out);
    reply(c, out);
    reply(c, "\n");
}

static void h_flush(conn *c, sess_request *r, const unsigned char *arg) {
    SessFlush(g_table, arg, r->arg_len);
    reply(c, "OK\n");
}

typedef void (*op_fn)(conn *, sess_request *, const unsigned char *);
static op_fn g_ops[7] = { h_open, h_use_run, h_use_set, h_use_append, h_close, h_stat, h_flush };

/* ---- per-connection request loop ----------------------------------------- */

static void serve(conn *c) {
    c->cur = NULL;
    for (;;) {
        sess_request req;
        if (conn_read_full(c, &req, sizeof req) != (int)sizeof req) break;
        if (req.arg_len > SESS_MAX_ARG) { reply(c, "ERR big\n"); break; }

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

/* ---- plugins ------------------------------------------------------------- */

static void load_plugins(void) {
    static const wchar_t *names[2] = { L"handler_echo.dll", L"handler_calc.dll" };
    for (int i = 0; i < 2; i++) {
        HMODULE m = LoadLibraryExW(names[i], NULL,
                                   LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (m) g_plugins[i] = (SessHandlerFn)GetProcAddress(m, HANDLER_ENTRY);
    }
}

/* ---- pipe server thread -------------------------------------------------- */

static unsigned __stdcall pipe_thread(void *arg) {
    (void)arg;
    for (;;) {
        HANDLE h = CreateNamedPipeW(SESS_PIPE_NAME, PIPE_ACCESS_DUPLEX,
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
    a.sin_port = htons(SESS_TCP_PORT);

    BOOL yes = TRUE;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof yes);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) != 0) { closesocket(ls); return 1; }
    if (listen(ls, SOMAXCONN) != 0) { closesocket(ls); return 1; }

    printf("sessiond: TCP 127.0.0.1:%d, pipe %ls\n", SESS_TCP_PORT, SESS_PIPE_NAME);
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
    g_table = SessNew();
    if (!g_table) { WSACleanup(); return 1; }
    load_plugins();

    _beginthreadex(NULL, 0, pipe_thread, NULL, 0, NULL);
    int rc = tcp_loop();

    WSACleanup();
    return rc;
}
