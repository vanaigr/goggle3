#include<stdint.h>
#include<cstring>
// #include<stdio.h>
#include"defs.h"
#include"alloc.h"

static char tmp_buf[tmp_max_c];

char *tmp = tmp_buf;
char *const tmp_end = tmp_buf + tmp_max_c;

char *align(char *p, int align_pow) {
    var res = (((uintptr_t)tmp >> align_pow) << align_pow);
    if(res != (uintptr_t)tmp) res += 1 << align_pow;
    return (char*)res;
}

char *alloc(int size, int align_pow) {
    // printf("allocated %d bytes with %d align\n", size, 1 << align_pow);
    let res = align(tmp, align_pow);
    tmp = res + size;
    return res;
}

bool streq(str a, str b) {
    return a.count == b.count && memcmp(a.items, b.items, a.count) == 0;
}
