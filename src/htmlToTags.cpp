#include <algorithm>
#include<cstring>
#include<stdio.h>

#include"defs.h"
#include"alloc.h"

#include"utf/decoder.h"
#define PRINT 0

char const *find(char const *b, char const *e, char c) {
    let res = memchr(b, c, e - b);
    if(res == NULL) return e;
    return (char*)res;
}

#define ARR_COUNT(arr) ((sizeof arr) / (sizeof arr[0]))

constexpr static bool is_self_closing(char const *tag, int tag_c) {
    if(tag_c == 2 && memcmp(tag,  "br", 2) == 0) return true;
    if(tag_c == 3 && memcmp(tag, "img", 3) == 0) return true;
    return false;
}

struct Attrs {
    Attr *items;
    int count;
    int cap;
};

static Attr *attrs_add(Attrs &attrs) {
    if(attrs.count == attrs.cap) {
        attrs.cap <<= 1;
        attrs.items = (Attr*)realloc(attrs.items, sizeof(Attr) * attrs.cap);
    }
    return attrs.items + attrs.count++;
}

static void recPrint(Tag *tag, int lvl) {
    for(var i = 0; i < lvl; i++) printf("  ");

    printf(
        "%.*s (0x%x - 0x%x)\n",
        tag->name.count,
        tag->name.items,
        (unsigned)(long long)tag->begin,
        (unsigned)(long long)tag->end
    );

    var c = tag + 1;
    let e = tag->descendants_e;
    while(c < e) {
        recPrint(c, lvl + 1);
        c = c->descendants_e;
    }
}

static char const *processAttrs(Attrs &attrs, char const *data, char const *end) {
    var cur = data;

    while(cur < end) {
        while(cur < end && *cur == ' ') cur++;
        if(cur == end) break;
        if(*cur == '>') break;

        let nameBeg = cur;
        while(cur < end) {
            let c = *cur;
            if(c == '=' || c == '>' || c == ' ') break;
            cur++;
        }
        let nameEnd = cur;

        while(cur < end && *cur == ' ') cur++;

        if(cur < end && *cur == '=') {
            cur++;
            while(cur < end && *cur == ' ') cur++;

            var quoted = false;
            if(cur < end) {
                let quote = *cur;
                if(quote == '"' || quote == '\'') {
                    quoted = true;
                    cur++;

                    let valueBeg = cur;
                    var backslash = false;
                    while(cur < end) {
                        let c = *cur;
                        if(!backslash && c == quote) break;
                        backslash = !backslash && c == '\\';
                        cur++;
                    }
                    let valueEnd = cur;
                    if(cur < end) cur++;

                    *attrs_add(attrs) = {
                        { nameBeg, (int)(nameEnd - nameBeg) },
                        { valueBeg, (int)(valueEnd - valueBeg) },
                    };
                }
            }

            if(!quoted) {
                printf(
                    "Unquoted attribute %.*s\n",
                    (int)(nameEnd - nameBeg),
                    nameBeg
                );
                continue;
            }
        }
        else {
            *attrs_add(attrs) = {
                { nameBeg, (int)(nameEnd - nameBeg) },
                { nameEnd, 0 },
            };
        }
    }

    return cur;
}

Tags htmlToTags(char const *data, int len) {
    let end = data + len;
    var current = data;

    // skip doctype
    current = find(current, end, '<');
    if(current < end) current++;

    var attrs = Attrs{ (Attr*)malloc(sizeof(Attr) * 1024), 0, 1024 };

    let stack = talloc<Tag*>(1024);
    var stack_c = 0;

    let tags = (Tag*)align(tmp, 6);
    var tags_c = 0;

    while(current < end) {
        let begin = find(current, end, '<');
        var cur = begin;
        if(end - cur > 1 && cur[1] == '!') {
            cur++;
            cur = find(cur, end, '>');
        }
        if(!(end - cur > 1 && cur[1] == '/')) {
            let nameBeg = cur + 1;
            var nameEnd = nameBeg;

            let attrBeg = attrs.count;
            var attrEnd = attrs.count;

            cur = nameBeg;
            while(cur < end) {
                let c = *cur;
                if(c == ' ' || c == '>') {
                    nameEnd = cur;
                    if(c == ' ') {
                        cur = processAttrs(attrs, cur, end);
                        if(cur < end) cur++;
                        attrEnd = attrs.count;
                    }
                    else {
                        cur++;
                    }

                    break;
                }
                cur++;
            }

            let content_beg = cur;
            let name_c = (int)(nameEnd - nameBeg);

            if(name_c == 0) {
                printf(
                    "not a tag `%.*s`...\n",
                    std::min<int>(5, end - nameEnd),
                    nameBeg
                );
                printf("Unexpected end of file\n");
            }
            else {
                let script = name_c == 6 && memcmp(nameBeg,  "script", 6) == 0;
                let style = name_c == 5 && memcmp(nameBeg,  "style", 5) == 0;
                let self_closing = is_self_closing(nameBeg, name_c);
            #if PRINT
                for(var i = 0; i < stack_c; i++) {
                    printf(" ");
                }
                printf(
                    "<%.*s (%d attrs)%s>\n",
                    name_c,
                    nameBeg,
                    attrEnd - attrBeg,
                    (self_closing ? "/" : "")
                );
            #endif
                var &t = tags[tags_c];
                tags_c++;
                t = Tag{
                    .begin = begin,
                    .name = { nameBeg, name_c },
                    .content_beg = content_beg,
                    .content_end = content_beg,
                    .end = content_beg,
                    .descendants_e = tags + tags_c,
                    .attrs_beg = attrBeg,
                    .attrs_end = attrEnd,
                };

                if(self_closing);
                else if(script || style) {
                    let len = script ? 8 : 7;
                    let cmp = script ? "/script>" : "/style>";

                    var found = false;
                    while(cur < end) {
                        cur = find(cur, end, '<');
                        if(end - cur >= len + 1) {
                            if(memcmp(cur + 1, cmp, len) == 0) {
                                t.content_end = cur;
                                cur = cur + 1 + len;
                                found = true;
                                break;
                            }
                            cur++;
                        }
                        else {
                            cur = end;
                        }
                    }

                    if(!found) {
                        t.content_end = cur;
                    }
                }
                else {
                    stack[stack_c] = &t;
                    stack_c++;
                }
            }
        }
        else if(end - cur > 0) {
            cur += 2;
            let end_tag_b = cur;
            let end_tag_e = find(cur, end, '>');
            let end_tag_c = (int)(end_tag_e - end_tag_b);
            if(end_tag_e == end) {
                printf("invalid closing tag %.*s\n", (int)(end - end_tag_b), end_tag_b);
                cur = end;
            }
            else {
                var stop_tag = stack_c - 1;
                while(stop_tag != -1) {
                    let &t = *stack[stop_tag];
                    if(streq(t.name, { end_tag_b, end_tag_c })) {
                        break;
                    }
                    stop_tag--;
                }

                if(stop_tag == -1) {
                    printf("Error: tag %.*s not found\n", end_tag_c, end_tag_b);
                }
                else {
                    for(var i = stack_c - 1; i != stop_tag - 1; i--) {
                        var &t = *stack[i];
                        t.descendants_e = tags + tags_c;
                        t.content_end = begin;
                        t.end = end_tag_e + 1;
                    #if PRINT
                        for(var j = 0; j < i; j++) {
                            printf(" ");
                        }
                        printf("</%.*s>\n", t.name.count, t.name.items);
                    #endif
                    }
                    stack_c = stop_tag;
                    if(stack_c == 0) {
                        // We know there's only one root html element.
                        // rest is who knows.
                        break;
                    }
                }
            }
        }

        current = cur;
    }

    for(var i = 0; i < stack_c; i++) {
        var &t = *stack[i];
        t.descendants_e = tags + tags_c;
        t.content_end = end;
        for(var j = 0; j < i; j++) {
            printf(" ");
        }
        printf("(this shouldn't happen)</%.*s>\n", t.name.count, t.name.items);
    }

    // recPrint(tags, 0);
        let printBinary = [](unsigned int num) {
            for (int i = 7; i >= 0; i--) {
                printf("%d", (num >> i) & 1);
                if(i == 4) printf("'");
            }
        };


    var p = data;
    while(p < end) {
        let c = utf_ptr2CharInfo(p);
        if(c.code < 0) {
            /*printBinary(p[-1]);
            printf(" ");
            printBinary(p[0]);
            printf(" ");
            printBinary(p[1]);
            printf(" ");
            printBinary(p[2]);
            printf(" ");
            printBinary(p[3]);
            printf("\n");*/

            printf(
                "%x %x %x %x\n",
                (unsigned char)p[0],
                (unsigned char)p[1],
                (unsigned char)p[2],
                (unsigned char)p[3]
            );
            fflush(stdout);
        }
        p += c.len;
    }

    tmp = (char*)(tags + tags_c);
    return { .items = tags, .count = tags_c, .attrs = attrs.items };
}
