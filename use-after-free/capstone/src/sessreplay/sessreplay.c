/* sessreplay.exe — local CLI: replay a recorded op-log (.rvn) against a fresh store.
 *
 * Usage: sessreplay <oplog.rvn>
 *
 * The local-file frontend: untrusted input is the op-log named on the command
 * line. It replays each recorded operation through sesscore, exactly as the
 * server would, so a crafted log reaches the same lifetime sinks.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "../sesscore/sesscore.h"
#include "../common/oplog.h"
#include "../common/protocol.h"
#include "../common/handler.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NPLUGINS 16
static SessHandlerFn g_plugins[NPLUGINS];

static void load_plugins(void) {
    static const wchar_t *names[2] = { L"handler_echo.dll", L"handler_calc.dll" };
    for (int i = 0; i < 2; i++) {
        HMODULE m = LoadLibraryExW(names[i], NULL,
                                   LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (m) g_plugins[i] = (SessHandlerFn)GetProcAddress(m, HANDLER_ENTRY);
    }
}
static SessHandlerFn plugin_for(uint8_t id) { return (id < NPLUGINS) ? g_plugins[id] : NULL; }

static int rd(FILE *f, void *b, size_t n) { return fread(b, 1, n, f) == n; }

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) { fwprintf(stderr, L"usage: sessreplay <oplog.rvn>\n"); return 2; }

    FILE *f = _wfopen(argv[1], L"rb");
    if (!f) { fwprintf(stderr, L"sessreplay: cannot open %s\n", argv[1]); return 1; }

    oplog_header h;
    if (!rd(f, &h, sizeof h) ||
        h.magic[0] != OPLOG_MAGIC0 || h.magic[1] != OPLOG_MAGIC1 ||
        h.magic[2] != OPLOG_MAGIC2 || h.magic[3] != OPLOG_MAGIC3) {
        fwprintf(stderr, L"sessreplay: bad op-log\n"); fclose(f); return 1;
    }
    if (h.count > OPLOG_MAX_RECORDS) { fclose(f); return 1; }

    load_plugins();
    SessTable *t = SessNew();
    if (!t) { fclose(f); return 1; }

    char out[256];
    for (uint32_t i = 0; i < h.count; i++) {
        uint8_t  op, type;
        uint16_t handle;
        uint32_t arg_len;
        if (!rd(f, &op, 1) || !rd(f, &type, 1) || !rd(f, &handle, 2) || !rd(f, &arg_len, 4)) break;
        if (arg_len > OPLOG_MAX_ARG) break;

        unsigned char *arg = NULL;
        if (arg_len) {
            arg = (unsigned char *)malloc(arg_len);
            if (!arg) break;
            if (!rd(f, arg, arg_len)) { free(arg); break; }
        }

        switch (op) {
            case OP_OPEN: {
                int id = SessOpen(t, type & HANDLER_ID_MASK, plugin_for(type & HANDLER_ID_MASK), arg, arg_len);
                printf("OPEN -> %d\n", id);
            } break;
            case OP_USE_RUN: {
                int n = SessUseRun(t, (int)handle, arg, arg_len, out, sizeof out);
                if (n >= 0) { fwrite(out, 1, (size_t)n, stdout); printf("\n"); }
                if (type & FLAG_DEFER) SessDefer(t, (int)handle);
            } break;
            case OP_USE_SET: {
                SessUseSet(t, (int)handle, arg, arg_len);
                if (type & FLAG_DEFER) SessDefer(t, (int)handle);
            } break;
            case OP_USE_APPEND:
                SessUseAppend(t, (int)handle, arg, arg_len);
                break;
            case OP_CLOSE:
                SessClose(t, (int)handle);
                break;
            case OP_STAT: {
                SessStat(t, (type & FLAG_VERBOSE) ? 1 : 0, out, sizeof out);
                printf("%s\n", out);
            } break;
            case OP_FLUSH:
                SessFlush(t, arg, arg_len);
                break;
            default:
                break;
        }
        free(arg);
    }

    fclose(f);
    SessTableFree(t);
    return 0;
}
