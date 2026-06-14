#include <string.h>

/* Classic stack buffer overflow.
 * buf is 16 bytes; strcpy copies until the NUL in `input`,
 * with no regard for the destination size. An input longer
 * than 15 bytes (+NUL) overruns buf and walks up the frame
 * toward saved RBP and the return address. */
void vuln(const char *input) {
    char buf[16];
    strcpy(buf, input);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
