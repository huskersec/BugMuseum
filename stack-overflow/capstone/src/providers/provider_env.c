/* provider_env.dll — load a profile from environment variables.
 *
 * spec is a ';'-separated list of variable names. Each present variable
 * becomes a CFG_STR entry. An empty/NULL spec imports a small default set.
 */
#define _CRT_SECURE_NO_WARNINGS
#define PROVIDER_EXPORTS
#include "../common/provider.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

static const char *DEFAULTS = "USERNAME;COMPUTERNAME;PROCESSOR_ARCHITECTURE";

/* Convert a wide name to a narrow (system codepage) name, bounded. */
static void narrow(const wchar_t *w, char *out, int outsz) {
    int n = WideCharToMultiByte(CP_ACP, 0, w, -1, out, outsz, NULL, NULL);
    if (n <= 0) out[0] = '\0';
    out[outsz - 1] = '\0';
}

PROVIDER_API int ProviderLoad(const wchar_t *spec, CfgProfile **out) {
    char list[1024];
    char wide_spec[1024];
    CfgProfile *p;
    char *ctx = NULL, *tok;

    if (!out) return CFG_EIO;

    if (spec && spec[0]) {
        narrow(spec, wide_spec, (int)sizeof(wide_spec));
        strncpy(list, wide_spec, sizeof(list) - 1);
    } else {
        strncpy(list, DEFAULTS, sizeof(list) - 1);
    }
    list[sizeof(list) - 1] = '\0';

    p = CfgNew();
    if (!p) return CFG_ENOMEM;

    for (tok = strtok_s(list, ";", &ctx); tok; tok = strtok_s(NULL, ";", &ctx)) {
        char val[2048];
        DWORD n = GetEnvironmentVariableA(tok, val, (DWORD)sizeof(val));
        if (n > 0 && n < sizeof(val)) {
            CfgAdd(p, tok, CFG_STR, (const unsigned char *)val, n);
        }
    }

    *out = p;
    return CFG_OK;
}
