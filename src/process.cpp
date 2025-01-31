#include<iostream>
#include<cstring>
#include<cassert>

#include"alloc.h"
#include"defs.h"

#if 0
    Tag const *title;
    Tag const *site_name;
    Tag const *site_display_url;
    str rawUrl;
    Tag const *desc;
#endif


template<typename P>
static void innerText(Tag const *t, P const &processor) {
    var text_begin = t->content_beg;

    var d = t + 1;
    let end = t->descendants_e;
    while(d < end) {
        processor(str{ text_begin, (int)(d->begin - text_begin) });
        innerText(d, processor);
        text_begin = d->end;
        d++;
    }

    processor(str{ text_begin, (int)(t->content_end - text_begin) });
}

struct Props {
    bool bold;
    bool italic;
};

static FormattedStr *addFormatted(char const *b, char const *e, Props props) {
    var cur = talloc<FormattedStr>(1);
    cur->bold = props.bold;
    cur->italic = props.italic;

    // TODO: unescaping
    cur->len = (int)(e - b);
    memcpy(cur->str, b, cur->len);
    tmp += cur->len;

    return cur;
}

static FormattedStr const **formattedInnerText(
    Tag const *t,
    FormattedStr const **begin,
    Props props = {}
) {
    var text_begin = t->content_beg;

    if(streq(t->name, STR("em"))) props.italic = true;
    if(streq(t->name, STR("b"))) props.bold = true;

    var prev = begin;

    var d = t + 1;
    let end = t->descendants_e;
    while(d < end) {
        let ptmp = tmp;
        var cur = addFormatted(text_begin, d->begin, props);
        if(cur->len == 0) {
            tmp = ptmp;
        }
        else {
            *prev = cur;
            prev = &cur->next;
        }

        prev = formattedInnerText(d, prev, props);

        text_begin = d->end;
        d++;
    }

    let ptmp = tmp;
    var cur = addFormatted(text_begin, t->content_end, props);
    if(cur->len == 0) {
        tmp = ptmp;
    }
    else {
        *prev = cur;
        prev = &cur->next;
    }

    return prev;
}

static void bodystr(str chunk) {
    // for now no unescaping.
    memmove(tmp, chunk.items, chunk.count);
    tmp += chunk.count;
}

static PResult process(Result r) {
    let titleBeg = tmp;
    innerText(r.title, bodystr);
    let titleEnd = tmp;
    let siteNameBeg = tmp;
    innerText(r.site_name, bodystr);
    let siteNameEnd = tmp;

    let siteDisplayUrlBeg = tmp;
    innerText(r.site_display_url, bodystr);
    let siteDisplayUrlEnd = tmp;

    var desc = (FormattedStr const *)nullptr;
    formattedInnerText(r.desc, &desc);

    return {
        .title = mkstr(titleBeg, titleEnd),
        .site_name = mkstr(siteNameBeg, siteNameEnd),
        .site_display_url = mkstr(siteDisplayUrlBeg, siteDisplayUrlEnd),
        .url = r.rawUrl,
        .desc = desc,
    };
}

PResults processResults(Results in) {
    let res = talloc<PResult>(in.count);
    for(var i = 0; i < in.count; i++) {
        res[i] = process(in.items[i]);
    }
    return { .items = res, .count = in.count };
}
