#include<stdint.h>
#include"defs.h"
#include"alloc.h"

static char tmp_buf[tmp_max_c];

char *tmp = tmp_buf;
char *const tmp_end = tmp_buf + tmp_max_c;

char *alloc(int size, int align_pow) {
    var res = (((uintptr_t)tmp >> align_pow) << align_pow);
    if(res != (uintptr_t)tmp) res += 1 << align_pow;

    tmp = (char*)res + size;
    return (char*)res;
}
