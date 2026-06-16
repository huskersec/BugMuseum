/* provider.h — Cascade source-provider plugin ABI.
 *
 * A provider is a DLL that loads a profile from one kind of source. The host
 * (e.g. regimport) selects a provider at runtime by source kind, resolves
 * "ProviderLoad" with GetProcAddress, and calls it. 'spec' is provider-specific:
 *   file: a path to a .cprof bundle
 *   reg:  a registry subkey path under HKCU (e.g. L"Software\\Cascade\\Demo")
 *   env:  a ';'-separated list of variable names (empty = a default set)
 */
#ifndef CASCADE_PROVIDER_H
#define CASCADE_PROVIDER_H

#include "../cfgcore/cfgcore.h"

#ifdef PROVIDER_EXPORTS
#define PROVIDER_API __declspec(dllexport)
#else
#define PROVIDER_API __declspec(dllimport)
#endif

#define PROVIDER_ENTRY "ProviderLoad"

typedef int (*ProviderLoadFn)(const wchar_t *spec, CfgProfile **out);

PROVIDER_API int ProviderLoad(const wchar_t *spec, CfgProfile **out);

#endif /* CASCADE_PROVIDER_H */
