/* spoolc.exe — local CLI: replay a recorded job-batch (.enc) against a fresh store.
 *
 * Usage: spoolc <batch.enc>
 *
 * The local-file frontend: untrusted input is the batch file named on the
 * command line. It replays each recorded operation through spoolcore, exactly as
 * the server would, so a crafted batch reaches the same lifetime sinks. Unlike
 * the network daemon, this is a synchronous one-shot tool: it submits, renders,
 * and appends, but does not run the background render pump.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "../spoolcore/spoolcore.h"
#include "../common/batch.h"
#include "../common/protocol.h"
#include "../common/filter.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFILTERS 3
static FilterFn g_filters[NFILTERS];

static void load_filters(void) {
    /* the CLI offers every format, including the consuming text filter */
    static const wchar_t *names[NFILTERS] = { L"filter_raster.dll", L"filter_text.dll", L"filter_pdfish.dll" };
    for (int i = 0; i < NFILTERS; i++) {
        HMODULE m = LoadLibraryExW(names[i], NULL,
                                   LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (m) g_filters[i] = (FilterFn)GetProcAddress(m, FILTER_ENTRY);
    }
}
static FilterFn filter_for(uint8_t id) { return (id < NFILTERS) ? g_filters[id] : NULL; }

static int rd(FILE *f, void *b, size_t n) { return fread(b, 1, n, f) == n; }

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) { fwprintf(stderr, L"usage: spoolc <batch.enc>\n"); return 2; }

    FILE *f = _wfopen(argv[1], L"rb");
    if (!f) { fwprintf(stderr, L"spoolc: cannot open %s\n", argv[1]); return 1; }

    batch_header h;
    if (!rd(f, &h, sizeof h) ||
        h.magic[0] != BATCH_MAGIC0 || h.magic[1] != BATCH_MAGIC1 ||
        h.magic[2] != BATCH_MAGIC2 || h.magic[3] != BATCH_MAGIC3) {
        fwprintf(stderr, L"spoolc: bad batch file\n"); fclose(f); return 1;
    }
    if (h.count > BATCH_MAX_RECORDS) { fclose(f); return 1; }

    load_filters();
    SpoolTable *t = SpoolNew();
    if (!t) { fclose(f); return 1; }

    char out[256];
    for (uint32_t i = 0; i < h.count; i++) {
        uint8_t  op, fmt;
        uint16_t job;
        uint32_t arg_len;
        if (!rd(f, &op, 1) || !rd(f, &fmt, 1) || !rd(f, &job, 2) || !rd(f, &arg_len, 4)) break;
        if (arg_len > BATCH_MAX_ARG) break;

        unsigned char *arg = NULL;
        if (arg_len) {
            arg = (unsigned char *)malloc(arg_len);
            if (!arg) break;
            if (!rd(f, arg, arg_len)) { free(arg); break; }
        }

        switch (op) {
            case OP_SUBMIT: {
                uint8_t id8 = fmt & FILTER_ID_MASK;
                int id = JobSubmit(t, id8, filter_for(id8), arg, arg_len);
                printf("SUBMIT -> %d\n", id);
            } break;
            case OP_RENDER: {
                int n = JobRender(t, (int)job, out, sizeof out);
                if (n >= 0) { fwrite(out, 1, (size_t)n, stdout); printf("\n"); }
            } break;
            case OP_APPEND:
                JobAppend(t, (int)job, arg, arg_len);
                break;
            case OP_CANCEL:
                JobCancel(t, (int)job);
                break;
            case OP_STATUS: {
                JobStatus(t, (fmt & FLAG_VERBOSE) ? 1 : 0, out, sizeof out);
                printf("%s\n", out);
            } break;
            default:
                break;   /* RECYCLE/DUP/FLUSH are server-side ops; ignored on the CLI */
        }
        free(arg);
    }

    fclose(f);
    SpoolTableFree(t);
    return 0;
}
