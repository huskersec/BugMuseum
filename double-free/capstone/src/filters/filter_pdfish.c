/* filter_pdfish.dll — renders a job to a PDF-ish preview header.
 * Bounded (safe); it borrows the document buffer and does not free it. */
#define _CRT_SECURE_NO_WARNINGS
#define FILTER_EXPORTS
#include "../common/filter.h"
#include <stdio.h>

FILTER_API int SpoolFilter(unsigned char *data, uint32_t len, char *out, uint32_t out_cap) {
    unsigned head = len ? data[0] : 0;
    int w = _snprintf(out, out_cap, "%%PDF-1.4 preview: %u bytes, head=0x%02x", len, head);
    if (w < 0) w = 0;
    return w;
}
