/* angr / pypcode pseudo-C  —  cl /O2
 * Loop gone; the off-by-one is now a 16-byte block move plus one trailing byte. */

void vuln(long long a0)
{
    char v0[17];        // [rsp+0x00]   (decompiler widens to fit the 17-byte write)

    *(__int128 *)v0 = *(__int128 *)a0;        // bytes 0..15
    v0[16] = *(char *)(a0 + 16);              // byte 16  (one past a 16-byte buffer)
    return;
}
