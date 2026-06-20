/* filter_text.dll — renders a text document to a short preview.
 *
 * A streaming text filter: it copies a bounded preview of the document and then
 * releases the document buffer. By contract this filter *consumes* its input —
 * the caller hands the job's buffer in and the filter disposes of it once the
 * preview has been produced (so the buffer is not kept around after rendering).
 */
#define _CRT_SECURE_NO_WARNINGS
#define FILTER_EXPORTS
#include "../common/filter.h"
#include <stdlib.h>
#include <string.h>

FILTER_API int SpoolFilter(unsigned char *data, uint32_t len, char *out, uint32_t out_cap) {
    uint32_t n = 0;
    if (out_cap) {
        n = len < out_cap - 1 ? len : out_cap - 1;
        if (n) memcpy(out, data, n);
        out[n] = '\0';
    }
    free(data);                 /* streaming consume: the document buffer is ours to release */
    return (int)n;
}
