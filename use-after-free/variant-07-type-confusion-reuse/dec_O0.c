/* angr / pypcode pseudo-C  —  cl /Od */

struct Handler { void (*fn)(const char *); char name[24]; };

void vuln(long long a0)
{
    struct Handler *h;
    char           *reuse;

    h = malloc(32);
    h->fn = greet;
    strncpy(h->name, "guest", 23);
    free(h);                                  // h dangling
    reuse = malloc(32);                       // reuses h's chunk
    memcpy(reuse, a0, strnlen(a0, 32));       // attacker fills fn ptr (offset 0) + name
    h->fn(h->name);                           // indirect call through dangling h -> attacker bytes
    return;
}
