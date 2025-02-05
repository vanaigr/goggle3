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

void tryAddResult(Results &res, Tag const *begin, Attr const *attrs) {
    let &root = *begin;
    if(!streq(root.name, STR("div"))) err;
    if(root.descendants_e == begin + 1) err;

    #define gotoFirstChild(cont, expectName) do {\
        if(cont->descendants_e == cont + 1) err; \
        cont = cont + 1; \
        if(!streq(cont->name, STR(expectName))) err; \
    } while(0)

    var container = &root;
    gotoFirstChild(container, "div");
    gotoFirstChild(container, "div");
    gotoFirstChild(container, "div");

    var first_cont = container;
    gotoFirstChild(first_cont, "div");

    let cont = first_cont;

    let a = findFirstName(cont, cont->descendants_e, STR("a"));
    if(a == cont->descendants_e) err;

    var titleTag = a;
    gotoFirstChild(titleTag, "span");

    var websiteNameTag = titleTag->descendants_e;
    if(websiteNameTag == a->descendants_e) err;

    var second_cont = first_cont->descendants_e;
    if(second_cont == container->descendants_e) err;

    let desc = second_cont;

    str href;
    {
        var i = a->attrs_beg;
        let end = a->attrs_end;
        while(i < end) {
            let a = attrs[i];
            if(streq(a.name, STR("href"))) break;
            i++;
        }

        if(i == end) err;
        href = attrs[i].value;
    }

    res.items[res.count++] = {
        .title = titleTag,
        .site_display_url = websiteNameTag,
        .rawUrl = href,
        .desc = desc,
    };
}

static void addList(Tags tags, Results &res) {
    let it = &tags.items[0];
    let it_end = it->descendants_e;

    var body = it + 1;
    while(body < it_end && !streq(body->name, STR("body"))) {
        body = body->descendants_e;
    }
    if(body == it_end) err;
    if(body + 1 == body->descendants_e) err;

    var list = body + 1;
    var divCount = 0;
    while(list < body->descendants_e) {
        divCount += streq(list->name, STR("div"));
        if(divCount == 2) break;
        list = list->descendants_e;
    }

    if(list == body->descendants_e) err;
    if(list + 1 == list->descendants_e) err;

    var i = list + 1;
    let c_end = list->descendants_e;
    while(i < c_end) {
        if(streq(i->name, STR("div"))) {
            tryAddResult(res, i, tags.attrs);
        }
        i = i->descendants_e;
    }
}

Results extractResults(Tags tags) {
    assert(tags.count > 0);

    Results res = { .items = (Result*)align(tmp, 6), .count = 0 };
    addList(tags, res);
    tmp = (char*)(res.items + res.count);
    return res;
}
