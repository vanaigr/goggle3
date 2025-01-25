constexpr int tmp_max_c = 200000;

extern char *tmp;
extern char *const tmp_end;

char *alloc(int size, int align_pow = 0);

template<typename T>
inline T *talloc(int count) {
    return (T *)alloc(sizeof(T) * count, 6);
}
