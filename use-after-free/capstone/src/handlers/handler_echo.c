/* handler_echo.dll — echoes the USE_RUN argument back. Bounded (safe);
 * any lifetime bug that reaches it lives in the store/server, not here. */
#define _CRT_SECURE_NO_WARNINGS
#define HANDLER_EXPORTS
#include "../common/handler.h"
#include <string.h>

HANDLER_API int SessHandler(const unsigned char *in, uint32_t in_len,
                            char *out, uint32_t out_cap) {
    uint32_t n = in_len < out_cap ? in_len : out_cap;
    if (n) memcpy(out, in, n);
    return (int)n;
}
