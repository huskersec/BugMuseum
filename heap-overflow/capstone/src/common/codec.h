/* codec.h — Avalanche value-codec plugin ABI.
 *
 * A codec is a DLL that transforms a value before it is stored. The server
 * selects one at runtime by id (request flag bits) and resolves "CodecApply"
 * with GetProcAddress. A codec must respect out_cap and return the number of
 * bytes written (or -1 on error).
 */
#ifndef AVALANCHE_CODEC_H
#define AVALANCHE_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef CODEC_EXPORTS
#define CODEC_API __declspec(dllexport)
#else
#define CODEC_API __declspec(dllimport)
#endif

#define CODEC_ENTRY "CodecApply"

typedef int (*CodecApplyFn)(const unsigned char *in, uint32_t in_len,
                            unsigned char *out, uint32_t out_cap);

CODEC_API int CodecApply(const unsigned char *in, uint32_t in_len,
                         unsigned char *out, uint32_t out_cap);

#endif /* AVALANCHE_CODEC_H */
