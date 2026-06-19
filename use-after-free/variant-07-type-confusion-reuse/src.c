#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Use-after-free -> type confusion -> control-flow hijack.
 * A Handler holding a function pointer is freed. A new allocation of the SAME
 * size is then filled with attacker input — likely reusing the freed chunk.
 * Calling through the dangling Handler's fn pointer jumps to whatever bytes the
 * attacker placed at offset 0 of the reused memory: a UAF turned into an
 * indirect call to an attacker-chosen address. This is the exploitation bridge
 * for the whole class. `/guard:cf` rejects the non-CFG-valid target on the call;
 * without CFG, control transfers. */
struct Handler { void (*fn)(const char *); char name[24]; };

static void greet(const char *who) { printf("hi %s\n", who); }

void vuln(const char *input) {
    struct Handler *h = (struct Handler *)malloc(sizeof *h);
    h->fn = greet;
    strncpy(h->name, "guest", sizeof h->name - 1);
    h->name[sizeof h->name - 1] = '\0';
    free(h);                                          /* h is now dangling */

    char *reuse = (char *)malloc(sizeof *h);          /* likely the same chunk */
    memcpy(reuse, input, strnlen(input, sizeof *h));  /* attacker fills it; fn ptr is at offset 0 */

    h->fn(h->name);     /* indirect call through the dangling pointer -> attacker bytes */
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
