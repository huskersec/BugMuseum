/* regimport.exe — local helper: import a profile from a source provider.
 *
 * Usage: regimport <reg|env|file> <spec>
 *
 * Selects a provider DLL at runtime by source kind, resolves ProviderLoad with
 * GetProcAddress, and calls it. Imported entries are printed. The reg source
 * reads values from HKCU\<spec>; the env source reads named variables; the
 * file source reads a .cprof bundle.
 */
#define _CRT_SECURE_NO_WARNINGS
#include "../common/provider.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

static const wchar_t *dll_for(const wchar_t *kind) {
    if (_wcsicmp(kind, L"reg")  == 0) return L"provider_reg.dll";
    if (_wcsicmp(kind, L"env")  == 0) return L"provider_env.dll";
    if (_wcsicmp(kind, L"file") == 0) return L"provider_file.dll";
    return NULL;
}

int wmain(int argc, wchar_t **argv) {
    const wchar_t *dll;
    HMODULE mod;
    ProviderLoadFn load;
    CfgProfile *p = NULL;
    int rc;

    if (argc < 3) {
        fwprintf(stderr, L"usage: regimport <reg|env|file> <spec>\n");
        return 2;
    }

    dll = dll_for(argv[1]);
    if (!dll) {
        fwprintf(stderr, L"regimport: unknown source '%s'\n", argv[1]);
        return 2;
    }

    mod = LoadLibraryExW(dll, NULL,
                         LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!mod) {
        fwprintf(stderr, L"regimport: cannot load %s (%lu)\n", dll, GetLastError());
        return 1;
    }

    load = (ProviderLoadFn)GetProcAddress(mod, PROVIDER_ENTRY);
    if (!load) {
        fwprintf(stderr, L"regimport: %s has no %S\n", dll, PROVIDER_ENTRY);
        FreeLibrary(mod);
        return 1;
    }

    rc = load(argv[2], &p);
    if (rc != CFG_OK || !p) {
        fwprintf(stderr, L"regimport: provider failed (%d)\n", rc);
        FreeLibrary(mod);
        return 1;
    }

    printf("imported %u value(s)\n", p->count);
    for (CfgEntry *e = p->head; e; e = e->next) {
        char label[256];
        CfgFormatLabel(e, label, sizeof(label));
        printf("  %s\n", label);
    }

    CfgFree(p);
    FreeLibrary(mod);
    return 0;
}
