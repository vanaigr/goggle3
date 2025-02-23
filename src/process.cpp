#include<iostream>
#include<cstring>
#include<cassert>

#include"alloc.h"
#include"defs.h"

template<typename P>
static void innerText(Tag const *t, P const &processor) {
    var text_begin = t->content_beg;

    var d = t + 1;
    let end = t->descendants_e;
    while(d < end) {
        processor(str{ text_begin, (int)(d->begin - text_begin) });
        innerText(d, processor);
        text_begin = d->end;
        d = d->descendants_e;
    }

    processor(str{ text_begin, (int)(t->content_end - text_begin) });
}

struct Props {
    bool bold;
    bool italic;
};

static FormattedStr *addFormatted(char const *b, char const *e, Props props) {
    var cur = talloc<FormattedStr>(1);

    let str = tmp;
    let len = (int)(e - b);
    tmp += len;

    // TODO: unescaping
    memcpy(str, b, len);

    *cur = {
        .bold = props.bold,
        .italic = props.italic,
        .len = len,
        .str = str,
        .next = nullptr,
    };

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
        d = d->descendants_e;
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
    fflush(stdout);
    // for now no unescaping.
    memmove(tmp, chunk.items, chunk.count);
    tmp += chunk.count;
}

static PResult process(Result r, GLBuffer &iconsBuf) {
    let titleBeg = tmp;
    innerText(r.title, bodystr);
    let titleEnd = tmp;

    str url;
    {
        var b = r.rawUrl.items;
        var e = b + r.rawUrl.count;
        let ignore = STR("/url?q=");
        if(streq(
            { b, std::min(r.rawUrl.count, ignore.count) },
            ignore
        )) {
            b += ignore.count;
        }
        e = find(b, e, '&');

        url = mkstr(b, e);
    }

    let texture = decodeIcon(r.favicon_url, iconsBuf);

    let siteDisplayUrlBeg = tmp;
    innerText(r.site_display_url, bodystr);
    let siteDisplayUrlEnd = tmp;

    var desc = (FormattedStr const *)nullptr;
    formattedInnerText(r.desc, &desc);

    return {
        .title = mkstr(titleBeg, titleEnd),
        .site_display_url = mkstr(siteDisplayUrlBeg, siteDisplayUrlEnd),
        .url = url,
        .texture = texture,
        .desc = desc,
    };
}

PResults processResults(Results in, GLBuffer &iconsBuf) {
    let res = talloc<PResult>(in.count);
    for(var i = 0; i < in.count; i++) {
        res[i] = process(in.items[i], iconsBuf);
    }
    return { .items = res, .count = in.count };
}
