/* handler.h — Revenant session-handler plugin ABI.
 *
 * A handler is a DLL that processes a session's USE_RUN argument. The server
 * resolves "SessHandler" with GetProcAddress at startup and stores the function
 * pointer in each Session (chosen by the OPEN `type`).
 */
#ifndef REVENANT_HANDLER_H
#define REVENANT_HANDLER_H

#include <stdint.h>

#ifdef HANDLER_EXPORTS
#define HANDLER_API __declspec(dllexport)
#else
#define HANDLER_API __declspec(dllimport)
#endif

#define HANDLER_ENTRY "SessHandler"

typedef int (*SessHandlerFn)(const unsigned char *in, uint32_t in_len,
                             char *out, uint32_t out_cap);

HANDLER_API int SessHandler(const unsigned char *in, uint32_t in_len,
                            char *out, uint32_t out_cap);

#endif /* REVENANT_HANDLER_H */
