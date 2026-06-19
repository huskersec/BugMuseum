/* handler_calc.dll — sums the argument bytes and returns a decimal string.
 * Bounded (safe). */
#define _CRT_SECURE_NO_WARNINGS
#define HANDLER_EXPORTS
#include "../common/handler.h"
#include <stdio.h>

HANDLER_API int SessHandler(const unsigned char *in, uint32_t in_len,
                            char *out, uint32_t out_cap) {
    unsigned long sum = 0;
    for (uint32_t i = 0; i < in_len; i++) sum += in[i];
    int w = _snprintf(out, out_cap, "sum=%lu", sum);
    if (w < 0) w = 0;
    return w;
}
