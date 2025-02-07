constexpr int tmp_max_c = 2'000'000;

extern char *tmp;
extern char *const tmp_beg;
extern char *const tmp_end;

char *alloc(int size, int align_pow = 0);

char *align(char *p, int align_pow);

template<typename T>
inline T *talloc(int count) {
    return (T *)alloc(sizeof(T) * count, 6);
}
