/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie. Under /O2 /GS /guard:cf the indirect call is wrapped by a CFG guard
 * (__guard_dispatch_icall) that validates the target — visible as an extra check before the call. */

struct Handler { void (*fn)(const char *); char name[24]; };

void vuln(long long a0)
{
    struct Handler *h;
    char           *reuse;

    h = malloc(32);
    h->fn = greet;
    strncpy(h->name, "guest", 23);
    free(h);
    reuse = malloc(32);
    memcpy(reuse, a0, strnlen(a0, 32));
    (*h->fn)(h->name);
    return;
}
