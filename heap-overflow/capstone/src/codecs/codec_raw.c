/* codec_raw.dll — identity codec: copy the value through unchanged.
 * Respects out_cap (safe); any size mishandling lives downstream in the store. */
#define _CRT_SECURE_NO_WARNINGS
#define CODEC_EXPORTS
#include "../common/codec.h"
#include <string.h>

CODEC_API int CodecApply(const unsigned char *in, uint32_t in_len,
                         unsigned char *out, uint32_t out_cap) {
    uint32_t n = in_len < out_cap ? in_len : out_cap;
    memcpy(out, in, n);
    return (int)n;
}
