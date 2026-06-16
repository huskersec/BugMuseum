/* provider_file.dll — load a profile from a .cprof file on disk.
 * Thin wrapper over the core file reader. */
#define _CRT_SECURE_NO_WARNINGS
#define PROVIDER_EXPORTS
#include "../common/provider.h"

PROVIDER_API int ProviderLoad(const wchar_t *spec, CfgProfile **out) {
    if (!spec || !out) return CFG_EIO;
    return CfgOpenFile(spec, out);
}
