extern unsigned long long __stack_chk_guard;

long long vuln(long long a0)
{
    char v0;  // [bp-0x28]
    unsigned long v1;  // [bp-0x10]

    v1 = __stack_chk_guard;
    strcpy(&v0, a0);
    if (v1 != __stack_chk_guard)
    {
        __stack_chk_fail();
        return __stack_chk_fail();
    }
    return v1 - __stack_chk_guard;
}

