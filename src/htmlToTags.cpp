#include <algorithm>
#include<cstring>
#include<stdio.h>

#include"defs.h"
#include"alloc.h"

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


Tags htmlToTags(char const *data, int len) {
    let end = data + len;
    var current = data;

    current = find(current, end, '\n');

    let stack = talloc<Tag*>(1024);
    var stack_c = 0;

    let tags = (Tag*)align(tmp, 6);
    var tags_c = 0;

    while(current < end) {
        let begin = find(current, end, '<');
        var cur = begin;
        if(end - cur > 0 && cur[1] != '/') {
            let nameBeg = cur + 1;
            var nameEnd = nameBeg;

            cur = nameBeg;
            while(cur < end) {
                let c = *cur;
                if(c == ' ' || c == '>') {
                    nameEnd = cur;
                    if(c == ' ') {
                        cur++;
                        var backslash = false;
                        var in_which_quote = -999;
                        while(cur < end) {
                            let c = *cur;

                            if(in_which_quote != -999) {
                                if(!backslash && c == in_which_quote) {
                                    in_which_quote = -999;
                                }
                            }
                            else {
                                if(c == '\'' || c == '"') {
                                    in_which_quote = c;
                                }
                                if(c == '>') {
                                    break;
                                }
                            }

                            backslash = !backslash && c == '\\';
                            cur++;
                        }
                    }

                    cur++;
                    break;
                }
                cur++;
            }

            let content_beg = cur;
            let name_c = (int)(nameEnd - nameBeg);

            if(name_c == 0) {
                printf("not a tag %.*s...\n", std::min<int>(5, end - nameEnd), nameBeg);
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
                    "<%.*s%s>\n",
                    name_c,
                    nameBeg,
                    (self_closing ? "/" : "")
                );
            #endif
                var &t = tags[tags_c];
                tags_c++;
                t = Tag{
                    .begin = cur,
                    .name = { nameBeg, name_c },
                    .content_beg = content_beg,
                    .content_end = content_beg,
                    .end = content_beg,
                    .descendants_e = tags + tags_c,
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
                        printf("</%.*s>\n", t.name_c, t.name);
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

    tmp = (char*)(tags + tags_c);
    return { .items = tags, .count = tags_c };
}
