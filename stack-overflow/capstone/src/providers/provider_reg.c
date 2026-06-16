/* provider_reg.dll — load a profile from the Windows registry.
 *
 * spec is a subkey path under HKEY_CURRENT_USER (an optional "HKCU\\" prefix
 * is tolerated). Each REG_SZ value under the key becomes a CFG_WSTR entry.
 */
#define _CRT_SECURE_NO_WARNINGS
#define PROVIDER_EXPORTS
#include "../common/provider.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const wchar_t *strip_root(const wchar_t *spec) {
    if (wcsncmp(spec, L"HKCU\\", 5) == 0) return spec + 5;
    if (wcsncmp(spec, L"HKEY_CURRENT_USER\\", 18) == 0) return spec + 18;
    return spec;
}

static void narrow(const wchar_t *w, char *out, int outsz) {
    int n = WideCharToMultiByte(CP_ACP, 0, w, -1, out, outsz, NULL, NULL);
    if (n <= 0) out[0] = '\0';
    out[outsz - 1] = '\0';
}

PROVIDER_API int ProviderLoad(const wchar_t *spec, CfgProfile **out) {
    HKEY hKey;
    CfgProfile *p;
    DWORD idx = 0;

    if (!spec || !out) return CFG_EIO;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, strip_root(spec), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return CFG_EIO;

    p = CfgNew();
    if (!p) { RegCloseKey(hKey); return CFG_ENOMEM; }

    for (;;) {
        wchar_t name[256];
        DWORD   name_len = 256;
        DWORD   type = 0;
        BYTE    data[4096];
        DWORD   cb = sizeof(data);

        LONG r = RegEnumValueW(hKey, idx++, name, &name_len, NULL, &type, data, &cb);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;

        if (type == REG_SZ) {
            /* Copy the value text into a local buffer for normalization. The
             * registry reports the data size in cb; values are short labels. */
            wchar_t wbuf[128];
            wcsncpy(wbuf, (const wchar_t *)data, cb);

            {
                char nname[256];
                narrow(name, nname, (int)sizeof(nname));
                CfgAdd(p, nname, CFG_WSTR, (const unsigned char *)wbuf,
                       (uint32_t)((wcslen(wbuf) + 1) * sizeof(wchar_t)));
            }
        }
    }

    RegCloseKey(hKey);
    *out = p;
    return CFG_OK;
}
