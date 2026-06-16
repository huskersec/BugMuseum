/* cfgapply.exe — local CLI: load a .cprof file and print its entries.
 *
 * Usage: cfgapply <file.cprof>
 *
 * This is the local-file frontend: untrusted input is the bundle named on the
 * command line. It parses the bundle and pretty-prints each entry.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "../cfgcore/cfgcore.h"

#include <stdio.h>

int wmain(int argc, wchar_t **argv) {
    CfgProfile *p = NULL;
    int rc;

    if (argc < 2) {
        fwprintf(stderr, L"usage: cfgapply <file.cprof>\n");
        return 2;
    }

    rc = CfgOpenFile(argv[1], &p);
    if (rc != CFG_OK) {
        fwprintf(stderr, L"cfgapply: parse failed (%d)\n", rc);
        return 1;
    }

    printf("loaded %u entrie(s)\n", p->count);
    for (CfgEntry *e = p->head; e; e = e->next) {
        char label[256];
        CfgFormatLabel(e, label, sizeof(label));
        printf("  %s\n", label);
    }

    CfgFree(p);
    return 0;
}
