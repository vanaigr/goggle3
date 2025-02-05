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

    var first_cont = container;
    gotoFirstChild(first_cont, "div");

    let a = findFirstName(first_cont, first_cont->descendants_e, STR("a"));
    if(a == first_cont->descendants_e) err;

    var cur = a;
    gotoFirstChild(cur, "div");
    let inside_a_end = cur->descendants_e;

    if(cur + 1 == inside_a_end) err;
    let websiteNameTag = cur + 1;
    cur = websiteNameTag->descendants_e;

    if(cur == inside_a_end) err;
    let titleTag = cur;

    let desc = first_cont->descendants_e;
    if(desc == container->descendants_e) err;

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

void addResults(Tags tags, Results &res) {
    let it = &tags.items[0];
    let it_end = it->descendants_e;

    var body = it + 1;
    while(body < it_end) {
        if(streq(body->name, STR("body"))) break;
        body = body->descendants_e;
    }
    if(body == it_end) err;

    var main = body + 1;
    let body_end = body->descendants_e;
    while(main < body_end) {
        if(streq(main->name, STR("div"))) break;
        main = main->descendants_e;
    }
    if(main == body_end) err;

    // skip first div with the prompt
    var item = main + 1;
    let main_end = main->descendants_e;
    while(item < main_end) {
        if(streq(item->name, STR("div"))) break;
        item = item->descendants_e;
    }
    if(item == main_end) err;
    item++;

    while(item < main_end) {
        if(streq(item->name, STR("div"))) {
            tryAddResult(res, item, tags.attrs);
        }
        item = item->descendants_e;
    }
}

Results extractResults(Tags tags) {
    assert(tags.count > 0);

    Results res = { .items = (Result*)align(tmp, 6), .count = 0 };
    addResults(tags, res);
    tmp = (char*)(res.items + res.count);

    return res;
}
