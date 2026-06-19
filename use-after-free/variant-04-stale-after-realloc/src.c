#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Use-after-free via a stale pointer after realloc.
 * realloc may move the block to satisfy a larger request, freeing the old one.
 * Its return value is the (possibly new) address; the OLD pointer passed in is
 * dangling if a move happened. Here the code keeps using the old `buf` after
 * growing it — a use-after-free whenever realloc relocates. (Under Full Page
 * Heap, realloc always relocates, so this faults every time.) */
void vuln(const char *more) {
    char *buf = (char *)malloc(16);
    strcpy(buf, "id=");
    char *grown = (char *)realloc(buf, 16 + strlen(more) + 1);
    if (!grown) { free(buf); return; }
    strcat(buf, more);            /* BUG: uses the OLD pointer; should be `grown` */
    printf("%s\n", grown);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
