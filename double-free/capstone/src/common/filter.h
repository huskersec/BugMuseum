/* filter.h — Encore render-filter plugin ABI.
 *
 * A filter is a DLL that renders a job's document buffer to a short text
 * preview. The server resolves "SpoolFilter" with GetProcAddress at startup and
 * stores the function pointer in each Job (chosen by the SUBMIT `fmt`).
 */
#ifndef ENCORE_FILTER_H
#define ENCORE_FILTER_H

#include <stdint.h>

#ifdef FILTER_EXPORTS
#define FILTER_API __declspec(dllexport)
#else
#define FILTER_API __declspec(dllimport)
#endif

#define FILTER_ENTRY "SpoolFilter"

/* Render `data[0..len)` into `out` (a NUL-terminated preview of at most
 * out_cap bytes). Returns the number of bytes the filter consumed, or < 0 on
 * error. */
typedef int (*FilterFn)(unsigned char *data, uint32_t len, char *out, uint32_t out_cap);

FILTER_API int SpoolFilter(unsigned char *data, uint32_t len, char *out, uint32_t out_cap);

#endif /* ENCORE_FILTER_H */
