#include <stdio.h>

/* Axis B variant: the SAME bug as variant-01, via a different primitive.
 * sprintf with a "%s" conversion expands `input` into buf[16] with no
 * field-width cap, so it is just as unbounded as strcpy. The disguise:
 * sprintf "feels" like formatted, controlled output — but "%s" of an
 * attacker string is a raw, unbounded copy. */
void vuln(const char *input) {
    char buf[16];
    sprintf(buf, "%s", input);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
