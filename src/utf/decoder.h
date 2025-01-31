#pragma once
// https://gist.github.com/vanaigr/1bda4744a784424a9015d97cae4f4a74

#include<stdint.h>

#include"macros.h"

extern uint8_t const utf8len_tab[256];
extern uint32_t const utf8_decoder_corrections[5];

/// Convert a UTF-8 byte sequence to a Unicode code point.
/// Does not handle ascii.
///
/// @param corr   Value from 'utf8_decoder_corrections', indexed by 'len'.
/// @param len    Expected length of the sequence, 0 or 1 if invalid.
/// @param[in] p  Pointer to the UTF-8 byte sequence, null-terminated.
///
/// @return Decoded code point, some negative value if invalid.
int32_t mb_decode_utf8(uint32_t const corr, uint32_t const len, uint8_t const *const p)
    ATTR_PURE ATTR_NONNULL_ALL ATTR_WARN_UNUSED_RESULT;

typedef struct CharInfo {
  int32_t code;
  int len;
} CharInfo;

static inline CharInfo utf_ptr2CharInfo(char const *const p_in)
    ATTR_PURE ATTR_ALWAYS_INLINE ATTR_NONNULL_ALL ATTR_WARN_UNUSED_RESULT;

/// Convert a UTF-8 byte sequence to a Unicode code point.
/// Handles ascii, multibyte sequiences and illegal sequences.
/// Overlong sequences and invalid Unicode code points are considered VALID.
///
/// @param[in]  p_in  String to convert, null-terminated.
///
/// @return information about the character. When the sequence is illegal,
/// "value" is negative, "len" is 1.
static inline CharInfo utf_ptr2CharInfo(char const *const p_in)
{
    uint8_t const *const p = (uint8_t const*)p_in;
    uint8_t const first = *p;
    if(EXPECT(first < 0x80, 1)) {
        return (CharInfo){ .code = first, .len = 1 };
    }

    uint8_t len = utf8len_tab[first];
    uint32_t const corr = utf8_decoder_corrections[len];
    int32_t const code_point = mb_decode_utf8(corr, len, p);
    if(EXPECT(code_point < 0, 0)) len = 1;
    return (CharInfo){ .code = code_point, .len = len };
}
