#include<iostream>
#include<cstring>
#include<cassert>

#include"alloc.h"
#include"defs.h"

Tag const *findFirstName(Tag const *beg, Tag const *end, str name) {
    for(var i = beg; i != end; i++) {
        if(streq(i->name, name)) return i;
    }
    return end;
}

#define err do { printf("error on line %d\n", __LINE__); fflush(stdout); return; } while(0)

int tryAddResult(Results &res, Tag *begin, Attr const *attrs) {
#define rl return __LINE__

    let &root = *begin;
    if(!streq(root.name, STR("div"))) rl;
    if(root.descendants_e == begin + 1) rl;

    #define gotoFirstChild(cont, expectName) do {\
        if(cont->descendants_e == cont + 1) rl; \
        cont = cont + 1; \
        if(!streq(cont->name, STR(expectName))) rl; \
    } while(0)

    var container = &root;
    {
        var i = container->attrs_beg;
        let end = container->attrs_end;
        while(i < end) {
            if(streq(attrs[i].name, STR("jscontroller"))) break;
            i++;
        }

        if(i == end) rl;
    }

    gotoFirstChild(container, "div");

    var first_cont = container;
    gotoFirstChild(first_cont, "div");

    let &cont = *first_cont;

    let &a = *findFirstName(&cont, cont.descendants_e, STR("a"));
    if(&a == cont.descendants_e) rl;

    let &titleTag = *findFirstName(&a, a.descendants_e, STR("h3"));
    if(&titleTag == a.descendants_e) rl;

    let &iconCont = *findFirstName(&titleTag + 1, a.descendants_e, STR("span"));
    let &websiteNameTag = *findFirstName(&iconCont + 1, a.descendants_e, STR("span"));
    let &iconTag = *findFirstName(&iconCont + 1, a.descendants_e, STR("img"));

    let &cite = *findFirstName(&websiteNameTag + 1, a.descendants_e, STR("cite"));

    var desc = (Tag const *)nullptr;

    var cur = first_cont;
    while(true) {
        // TODO: no description? Maybe should allow...
        if(container->descendants_e == cur->descendants_e) rl;
        cur = cur->descendants_e;
        if(!streq(cur->name, STR("div"))) rl;

        var c = cur;
        gotoFirstChild(c, "div");

        var i = c + 1;
        let end = c->descendants_e;
        var allSpan = i != end;
        while(i < end) {
            if(!streq(i->name, STR("span"))) {
                allSpan = false;
                break;
            }
            i = i->descendants_e;
        }

        if(allSpan) {
            desc = c;
            break;
        }
    }

    str href;
    {
        var i = a.attrs_beg;
        let end = a.attrs_end;
        while(i < end) {
            let a = attrs[i];
            if(streq(a.name, STR("href"))) break;
            i++;
        }

        if(i == end) rl;
        href = attrs[i].value;
    }

    str favicon_url;
    {
        var i = iconTag.attrs_beg;
        let end = iconTag.attrs_end;
        while(i < end) {
            let a = attrs[i];
            if(streq(a.name, STR("src"))) break;
            i++;
        }

        if(i == end) {
            favicon_url = { nullptr, 0 };
        }
        else {
            favicon_url = attrs[i].value;
        }
    }

    res.items[res.count++] = {
        .title = &titleTag,
        //.site_name = &websiteNameTag,
        .site_display_url = &cite,
        .rawUrl = href,
        .favicon_url = favicon_url,
        .desc = desc,
    };

    return 0;
}

void searchResults(Results &res, Tag *tag, Attr *attrs, int lvl = 0) {
    let addErr = tryAddResult(res, tag, attrs);

    if(addErr && streq(tag->name, STR("div"))) {
        var i = tag + 1;
        let end = tag->descendants_e;
        while(i < end) {
            searchResults(res, i, attrs, lvl + 1);
            i = i->descendants_e;
        }
    }
}

Results extractResults(Tags tags) {
    assert(tags.count > 0);

    Results res = { .items = (Result*)align(tmp, 6), .count = 0 };
    searchResults(res, tags.items, tags.attrs);
    tmp = (char*)(res.items + res.count);

    return res;
}
