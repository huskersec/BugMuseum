#include <stdio.h>
#include <stdlib.h>

/* Axis B variant: the SAME heap bug as variant-01, via a different primitive.
 * sprintf with a "%s" conversion expands `input` into a 16-byte heap buffer
 * with no field-width cap, so it is just as unbounded as strcpy. The disguise:
 * sprintf "feels" like controlled, formatted output — but "%s" of an attacker
 * string is a raw, unbounded copy into the heap allocation. */
void vuln(const char *input) {
    char *buf = (char *)malloc(16);
    sprintf(buf, "%s", input);
    free(buf);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
