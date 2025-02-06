#include<stdio.h>
#include<stdint.h>
#include<cstring>

#include"alloc.h"
#include"defs.h"

// https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Set-Cookie
static let nameInvalid0 = STR(" ()<>@,;:\\\"/[]?={}");
static let nib = nameInvalid0.items;
static let nie = nameInvalid0.items + nameInvalid0.count;

static let valueInvalid0 = STR("\",;\\");
static let vib = valueInvalid0.items;
static let vie = valueInvalid0.items + valueInvalid0.count;

static bool validNameChar(char c0) {
    let c = (uint8_t)c0;
    if(c < 32) return false;
    if(c >= 127) return false;
    return find(nib, nie, c) == nie;
}

static bool validValueChar(char c0) {
    let c = (uint8_t)c0;
    if(c < 32) return false;
    if(c >= 127) return false;
    return find(vib, vie, c) == vie;
}

#define err do { printf("[COOKIE] error on line %d\n", __LINE__); fflush(stdout); return {}; } while(0)

Cookie parseOneCookie(char const *&cur, char const *end) {
    let nameBeg = cur;
    while(cur < end && validNameChar(*cur)) cur++;
    let nameEnd = cur;
    if(cur >= end || *cur != '=') err;
    cur++;

    let quoted = cur < end && *cur == '"';
    if(quoted) cur++;

    let valueBeg = cur;
    while(cur < end && validValueChar(*cur)) cur++;
    let valueEnd = cur;

    if(quoted) {
        if(cur >= end || *cur != '"') err;
        cur++;
    }

    return {
        .name = mkstr(nameBeg, nameEnd),
        .value = mkstr(valueBeg, valueEnd),
    };
}

#undef err

#define err do { printf("[COOKIE] error on line %d\n", __LINE__); fflush(stdout); return; } while(0)

static void processCookie(char const *beg, char const *end, Cookies &res) {
    var cur = beg;
    while(cur < end) {
        while(cur < end && *cur == ' ') cur++;
        if(cur == end) break;
        {
            var check = cur;
            while(check < end && *check == '\n') check++;
            if(check == end) break;
            if(check != cur) err;
        }

        let c = parseOneCookie(cur, end);
        if(c.name.items == nullptr) return;

        if(cur < end && *cur != ';') {
            var check = cur;
            while(check < end && *check == '\n') check++;
            if(check == end) break;
            if(check != cur) err;
        }
        cur++;

        res.items[res.count++] = c;
    }
}

Cookies getCookies(char const *buf, int count) {
    var cookies = Cookies{ (Cookie*)align(tmp, 6), 0 };
    processCookie(buf, buf + count, cookies);
    tmp = (char*)(cookies.items + cookies.count);

    return cookies;
}

str encodeCookies(Cookies cookies) {
    let beg = tmp;
    var cur = beg;
    for(var i = 0; i < cookies.count; i++) {
        let a = cookies.items[i];
        memcpy(cur, a.name.items, a.name.count);
        cur += a.name.count;
        *cur++ = '=';
        memcpy(cur, a.value.items, a.value.count);
        cur += a.value.count;
        if(i != cookies.count - 1) *cur++ = ';';
    }
    tmp = cur;

    return mkstr(beg, cur);
}
