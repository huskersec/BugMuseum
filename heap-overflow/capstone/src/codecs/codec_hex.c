/* codec_hex.dll — decode an ASCII-hex value into raw bytes.
 * Output is at most in_len/2 bytes and is bounded by out_cap (safe). */
#define _CRT_SECURE_NO_WARNINGS
#define CODEC_EXPORTS
#include "../common/codec.h"

static int hexval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

CODEC_API int CodecApply(const unsigned char *in, uint32_t in_len,
                         unsigned char *out, uint32_t out_cap) {
    uint32_t o = 0;
    for (uint32_t i = 0; i + 1 < in_len && o < out_cap; i += 2) {
        int hi = hexval(in[i]);
        int lo = hexval(in[i + 1]);
        if (hi < 0 || lo < 0) break;
        out[o++] = (unsigned char)((hi << 4) | lo);
    }
    return (int)o;
}
