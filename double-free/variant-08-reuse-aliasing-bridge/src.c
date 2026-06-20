#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free as an exploitation bridge — why it matters.
 * The earlier variants establish that a chunk can land on the freelist twice.
 * This one shows the payoff: a chunk on the freelist twice can be handed out
 * twice. After free(p) twice, two same-size allocations (h and alias) can be
 * backed by the SAME chunk. Now h and alias are two "distinct" objects that
 * overlap in memory: writing attacker bytes through `alias` overwrites h's
 * fields — including its function pointer `fn` (the first member). The final
 * indirect call `h->fn()` then jumps to an attacker-controlled address.
 *
 * This is non-deterministic: whether the two mallocs alias depends on the
 * allocator (LFH/segment-heap state, quarantine, page heap). Under Page Heap or
 * the segment heap's double-free detection the SECOND free fast-fails first
 * (STATUS_HEAP_CORRUPTION) — which is the defense. When it does alias, the
 * indirect call is the corruption sink: this is the one double-free shape where
 * /guard:cf is meaningful — CFG validates h->fn before the call and rejects a
 * target that is not a registered function entry. */
typedef struct {
    void (*fn)(void);          /* first member: the indirect-call target */
    char  name[24];
} Handler;                     /* sizeof == 32 on x64 */

static void legit(void) { /* the intended handler */ }

int vuln(const char *input) {
    /* prime the freelist with a double-freed chunk */
    char *p = (char *)malloc(sizeof(Handler));
    free(p);
    free(p);                                       /* on the freelist twice */

    /* two same-size allocations may now be backed by the same chunk */
    Handler *h     = (Handler *)malloc(sizeof(Handler));
    char    *alias = (char *)malloc(sizeof(Handler));   /* may overlap *h */

    h->fn = legit;                                 /* victim sets its handler */
    memcpy(alias, input, sizeof(Handler));         /* attacker bytes over h->fn */

    h->fn();                   /* indirect call through a now-attacker-influenced
                                  pointer -> the /guard:cf check site */
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
