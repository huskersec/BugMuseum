/* filter_raster.dll — renders a job to a raster preview (sums "ink" coverage).
 * Bounded (safe); it reads the document buffer but does not take ownership of
 * it — the store keeps owning the buffer. */
#define _CRT_SECURE_NO_WARNINGS
#define FILTER_EXPORTS
#include "../common/filter.h"
#include <stdio.h>

FILTER_API int SpoolFilter(unsigned char *data, uint32_t len, char *out, uint32_t out_cap) {
    unsigned long ink = 0;
    for (uint32_t i = 0; i < len; i++) ink += data[i];
    int w = _snprintf(out, out_cap, "raster: %u cells, ink=%lu", len, ink);
    if (w < 0) w = 0;
    return w;
}
