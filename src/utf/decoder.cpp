#include"decoder.h"

// uint8_t is a reminder for clang to use smaller cmp
#define IS_TRAILING_VALID(byte) ((uint8_t)((byte) & 0xC0u) == 0x80u)

// This is a modified version of utf_ptr2CharInfo_impl() from neovim repo in mbyte.c at 4860cc5
// that only handles 1-4 byte UTF-8 instead of 1-6.
int32_t mb_decode_utf8(uint32_t const corr, uint32_t const len, uint8_t const *const p) {
    int32_t result;
    uint8_t cur;

    // Reading second byte unconditionally, safe for invalid
    // as it cannot be the last byte, not safe for ascii.
    uint32_t code_point = ((uint32_t)p[0] << 6) + (cur = p[1]);

    if(EXPECT(!IS_TRAILING_VALID(cur), 0)) goto err;
    if(EXPECT(len == 2, 0)) goto ret;  // EXPECT for clang #1
    if(EXPECT(len < 2, 0)) goto err;

    code_point = (code_point << 6) + (cur = p[2]);
    if(EXPECT(!IS_TRAILING_VALID(cur), 0)) goto err;
    if(len == 3) goto ret;

    code_point = (code_point << 6) + (cur = p[3]);
    if(EXPECT(!IS_TRAILING_VALID(cur), 0)) goto err;
    ASSUME(len == 4);
    //if(code_point > 0x10FFFFu - utf8_decoder_corrections[4]) goto err;  // #2

ret:
    result = (int32_t)(corr + code_point);
    ASSUME(result >= 0);
    return result;

err:
    result = (int32_t)corr;
    ASSUME(result < 0);
    return result;
}

// Note: all values are negative
uint32_t const utf8_decoder_corrections[] = {
  (1u << 31),  // invalid - set invalid bits (safe to add as first 2 bytes
  (1u << 31),  // won't affect highest bit in normal ret. see #1 below)
  -(0x80u + (0xC0u << 6)),  // multibyte - subtract added UTF8 bits (1..10xxx and 10xxx)
  -(0x80u + (0x80u << 6) + (0xE0u << 12)),
  -(0x80u + (0x80u << 6) + (0x80u << 12) + (0xF0u << 18)),
};

uint8_t const utf8len_tab[] = {
  // ?1 ?2 ?3 ?4 ?5 ?6 ?7 ?8 ?9 ?A ?B ?C ?D ?E ?F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 1?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 2?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 3?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 4?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 5?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 6?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 7?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 8?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 9?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // A?
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // B?
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // C?
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // D?
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // E?
  4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // F?
};  // #3


// #1 You can remove this condition if the function is not likely
// to be inlined or you are not checking for code point being invalid.
// You can also then remove EXPECT 0. Don't forget to remove ASSUME(result >= 0).

// #2 If you are checking for whether the result is an invalid Unicode
// code point right away, you can uncomment this line.
// Correction is a separate parameter since you are probably going
// to return this value if a code point turn out to be invalid.

// #3 You can also use a table that has 0's for invalid bytes
