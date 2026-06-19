/* cached.exe — Avalanche cache server.
 *
 * Exposes the request protocol over two transports:
 *   - a named pipe  (\\.\pipe\avalanche)  for local clients
 *   - a TCP socket  (127.0.0.1:7000)      for networked clients
 *
 * Ops are dispatched through a handler table. The pipe serves the full lenient
 * command set (GET/DEL normalize keys, verbose STATS); the TCP listener is the
 * "fast" interface (exact GET, a fast inline SET path, terse STATS). The TCP
 * listener binds loopback only — intentionally-vulnerable research code, do not
 * expose on a network.
 */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include "../cachecore/cachecore.h"
#include "../common/protocol.h"
#include "../common/codec.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

static Cache            *g_cache;
static CRITICAL_SECTION  g_lock;            /* guards g_cache across pipe + TCP */

#define NCODECS 16
static CodecApplyFn      g_codecs[NCODECS]; /* resolved at startup via GetProcAddress */

typedef struct conn { int is_tcp; SOCKET sock; HANDLE pipe; } conn;

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

/* Build a NUL-terminated C string from raw bytes (bounded; caller frees). */
static char *cstr(const unsigned char *p, uint32_t n) {
    char *s = (char *)malloc((size_t)n + 1);
    if (!s) return NULL;
    if (n) memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

static CodecApplyFn codec_for(uint8_t id) { return (id < NCODECS) ? g_codecs[id] : NULL; }

/* ---- op handlers (reached via the dispatch table) ------------------------ */

static void handle_set(conn *c, cache_request *r, const char *key, const unsigned char *value) {
    if (c->is_tcp && (r->flags & FLAG_FAST)) {
        /* fast inline path: run the selected codec, then stash the result. */
        uint8_t cid = (uint8_t)((r->flags & FLAG_CODEC_MASK) >> FLAG_CODEC_SHIFT);
        CodecApplyFn codec = codec_for(cid);
        uint32_t vlen = r->value_len;
        unsigned char *work = (unsigned char *)malloc(vlen ? vlen : 1);
        if (!work) { reply(c, "ERR mem\n"); return; }
        int outlen;
        if (codec) outlen = codec(value, vlen, work, vlen);
        else { if (vlen) memcpy(work, value, vlen); outlen = (int)vlen; }

        CacheEntry *e = CacheInsertEmpty(g_cache, key);
        if (e && outlen >= 0) CacheStoreFast(e, work, (uint32_t)outlen);
        free(work);
        reply(c, "STORED\n");
    } else {
        CacheSet(g_cache, key, value, r->value_len);
        reply(c, "STORED\n");
    }
}

static void handle_get(conn *c, cache_request *r, const char *key, const unsigned char *value) {
    (void)r; (void)value;
    CacheEntry *e = c->is_tcp ? CacheGetRaw(g_cache, key) : CacheGet(g_cache, key);
    if (e) { reply(c, "VALUE "); conn_write(c, e->value, (int)e->value_len); reply(c, "\n"); }
    else reply(c, "NOT_FOUND\n");
}

static void handle_del(conn *c, cache_request *r, const char *key, const unsigned char *value) {
    (void)r; (void)value;
    if (c->is_tcp) { reply(c, "ERR unsupported\n"); return; }   /* DEL is local (pipe) only */
    reply(c, CacheDel(g_cache, key) == CACHE_OK ? "DELETED\n" : "NOT_FOUND\n");
}

static void handle_stats(conn *c, cache_request *r, const char *key, const unsigned char *value) {
    (void)key; (void)value;
    char hdr[64];
    _snprintf(hdr, sizeof hdr, "ENTRIES %u\n", g_cache->count);
    hdr[sizeof hdr - 1] = '\0';
    reply(c, hdr);
    for (uint32_t i = 0; i < g_cache->nbuckets; i++) {
        for (CacheEntry *e = g_cache->buckets[i]; e; e = e->next) {
            char *line = CacheFormatLine(e);
            if (line) { reply(c, line); reply(c, "\n"); free(line); }
            if (!c->is_tcp && (r->flags & FLAG_VERBOSE)) {
                char *p = CachePreviewKey(e->key);
                if (p) { reply(c, "  preview="); reply(c, p); reply(c, "\n"); free(p); }
            }
        }
    }
}

typedef void (*op_handler)(conn *, cache_request *, const char *, const unsigned char *);
static op_handler g_handlers[4] = { handle_set, handle_get, handle_del, handle_stats };

/* ---- request loop -------------------------------------------------------- */

static void serve(conn *c) {
    cache_request req;
    if (conn_read_full(c, &req, sizeof req) != (int)sizeof req) return;
    if (req.op > OP_STATS) { reply(c, "ERR op\n"); return; }
    if (req.value_len > AVL_MAX_PAYLOAD) { reply(c, "ERR too big\n"); return; }   /* key_len is u16, always <= 64K */

    char          *key   = NULL;
    unsigned char *value = NULL;

    if (req.key_len) {
        unsigned char *kb = (unsigned char *)malloc(req.key_len);
        if (!kb) return;
        if (conn_read_full(c, kb, req.key_len) != (int)req.key_len) { free(kb); return; }
        key = cstr(kb, req.key_len);
        free(kb);
    } else {
        key = cstr((const unsigned char *)"", 0);
    }
    if (!key) return;

    if (req.value_len) {
        value = (unsigned char *)malloc(req.value_len);
        if (!value) { free(key); return; }
        if (conn_read_full(c, value, (int)req.value_len) != (int)req.value_len) { free(key); free(value); return; }
    }

    EnterCriticalSection(&g_lock);
    g_handlers[req.op](c, &req, key, value);
    LeaveCriticalSection(&g_lock);

    free(key);
    free(value);
}

/* ---- codec loading ------------------------------------------------------- */

static void load_codecs(void) {
    static const wchar_t *names[2] = { L"codec_raw.dll", L"codec_hex.dll" };
    for (int i = 0; i < 2; i++) {
        HMODULE m = LoadLibraryExW(names[i], NULL,
                                   LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (m) g_codecs[i] = (CodecApplyFn)GetProcAddress(m, CODEC_ENTRY);
    }
}

/* ---- pipe server thread -------------------------------------------------- */

static unsigned __stdcall pipe_thread(void *arg) {
    (void)arg;
    for (;;) {
        HANDLE h = CreateNamedPipeW(AVL_PIPE_NAME, PIPE_ACCESS_DUPLEX,
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
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(AVL_TCP_PORT);

    BOOL yes = TRUE;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof yes);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) != 0) { closesocket(ls); return 1; }
    if (listen(ls, SOMAXCONN) != 0) { closesocket(ls); return 1; }

    printf("cached: TCP 127.0.0.1:%d, pipe %ls\n", AVL_TCP_PORT, AVL_PIPE_NAME);
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
    g_cache = CacheNew(1024);
    if (!g_cache) { WSACleanup(); return 1; }
    load_codecs();

    _beginthreadex(NULL, 0, pipe_thread, NULL, 0, NULL);
    int rc = tcp_loop();

    WSACleanup();
    return rc;
}
