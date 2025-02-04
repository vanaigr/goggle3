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

void tryAddResult(Results &res, Tag *begin, Attr const *attrs) {
    let &root = *begin;
    if(!streq(root.name, STR("div"))) return;
    if(root.descendants_e == begin + 1) return;

    #define gotoFirstChild(cont, expectName) do {\
        if(cont->descendants_e == cont + 1) return; \
        cont = cont + 1; \
        if(!streq(cont->name, STR(expectName))) return; \
    } while(0)

    var container = &root;
    while(true) {
        var i = container->attrs_beg;
        let end = container->attrs_end;
        while(i < end) {
            if(streq(attrs[i].name, STR("jscontroller"))) break;
            i++;
        }

        if(i != end) break;
        gotoFirstChild(container, "div");
    }

    gotoFirstChild(container, "div");

    var first_cont = container;
    gotoFirstChild(first_cont, "div");

    let &cont = *first_cont;

    let &a = *findFirstName(&cont, cont.descendants_e, STR("a"));
    if(&a == cont.descendants_e) return;

    let &titleTag = *findFirstName(&a, a.descendants_e, STR("h3"));
    if(&titleTag == a.descendants_e) return;

    let &iconCont = *findFirstName(&titleTag + 1, a.descendants_e, STR("span"));
    let &websiteNameTag = *findFirstName(&iconCont + 1, a.descendants_e, STR("span"));

    let &cite = *findFirstName(&websiteNameTag + 1, a.descendants_e, STR("cite"));

    var desc = (Tag const *)nullptr;

    var cur = first_cont;
    while(true) {
        // TODO: no description? Maybe should allow...
        if(container->descendants_e == cur->descendants_e) return;
        cur = cur->descendants_e;
        if(!streq(cur->name, STR("div"))) return;

        var c = cur;
        gotoFirstChild(c, "div");
        if(c->descendants_e == c + 1) {
            desc = c;
            break;
        }
        else if(c->descendants_e > c + 1) {
            let prev_c = c;
            c = c + 1;
            if(prev_c->descendants_e == c->descendants_e && streq(c->name, STR("span"))) {
                desc = c;
                break;
            }
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

        if(i == end) return;
        href = attrs[i].value;
    }

    res.items[res.count++] = {
        .title = &titleTag,
        .site_name = &websiteNameTag,
        .site_display_url = &cite,
        .rawUrl = href,
        .desc = desc,
    };
}

Results extractResults(Tags tags) {
    assert(tags.count > 0);

    Results res = { .items = (Result*)align(tmp, 6), .count = 0 };
    var i = tags.items + 1;
    let end = tags.items[0].descendants_e;
    while(i < end) {
        tryAddResult(res, i, tags.attrs);
        i = i->descendants_e;
    }
    tmp = (char*)(res.items + res.count);

    return res;
}
