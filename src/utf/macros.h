#pragma once

#if defined(__clang__) || defined(__GNUC__)
#   define ATTR_PURE               __attribute__((pure))
#   define ATTR_CONST              __attribute__((const))
#   define ATTR_NONNULL_ALL        __attribute__((nonnull))
#   define ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#   define ATTR_ALWAYS_INLINE      __attribute__((always_inline))
#else
#   define ATTR_PURE
#   define ATTR_CONST
#   define ATTR_NONNULL_ALL
#   define ATTR_WARN_UNUSED_RESULT
#   define ATTR_ALWAYS_INLINE
#endif

#if defined(__clang__) || defined(__GNUC__)
#   define EXPECT(cond, value) __builtin_expect((cond), (value))
#elif defined(_MSC_VER)
#   define EXPECT(cond, value) (cond)
#else
#   define EXPECT(cond, value) (cond)
#endif

#if defined(__clang__)
#   define ASSUME(cond) do { __builtin_assume((cond)); } while(0)
#elif defined(__GNUC__)
#   define ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while(0)
#elif defined(_MSC_VER)
#   define ASSUME(cond) do { __assume((cond)); } while(0)
#else
// if (!(cond)) &(char*)0 = 0;
#   define ASSUME(cond) do {} while(0)
#endif
