/* angr / pypcode pseudo-C  —  cl /Od */

struct Buffer { char *data; unsigned long long len; };

void vuln(long long a0)
{
    struct Buffer *b;
    char          *p;

    b = malloc(16);
    b->data = malloc(32);
    strncpy(b->data, a0, 31);
    b->data[31] = 0;
    b->len = strlen(b->data);
    p = b->data;                 // save pointer to owned data
    buffer_free(b);              // frees b->data and b
    printf("%s\n", p);           // p dangles into the freed owned allocation
    return;
}
