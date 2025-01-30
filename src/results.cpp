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

void tryAddResult(Results &res, Tag *begin) {
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
        var found = false;
        var pos = container->name.items + container->name.count;
        let end = container->content_beg;
        while(pos < end) {
            let cmp = STR("data-rpos");
            if(streq({ pos + 1, std::min<int>(cmp.count, end - (pos + 1)) }, cmp )) {
                found = true;
                break;
            }
            pos = find(pos + 1, end, ' ');
        }

        if(found) break;

        gotoFirstChild(container, "div");
    }

    gotoFirstChild(container, "div");
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

    str desc = { nullptr, 0 };

    var cur = first_cont;
    while(true) {
        // TODO: no description? Maybe should allow...
        if(container->descendants_e == cur->descendants_e) return;
        cur = cur->descendants_e;
        if(!streq(cur->name, STR("div"))) return;

        var c = cur;
        gotoFirstChild(c, "div");
        if(c->descendants_e == c + 1) {
            desc = { c->content_beg, (int)(c->content_end - c->content_beg) };
            break;
        }
        else if(c->descendants_e > c + 1) {
            let prev_c = c;
            c = c + 1;
            if(prev_c->descendants_e == c->descendants_e && streq(c->name, STR("span"))) {
                desc = { c->content_beg, (int)(c->content_end - c->content_beg) };
                break;
            }
        }
    }

    printf("found a result\n");
    printf(
        "  title is %.*s\n",
        (int)(titleTag.content_end - titleTag.content_beg),
        titleTag.content_beg
    );
    printf(
        "  website name is %.*s\n",
        (int)(websiteNameTag.content_end - websiteNameTag.content_beg),
        websiteNameTag.content_beg
    );
    printf(
        "  website url name is %.*s\n",
        (int)(cite.content_end - cite.content_beg),
        cite.content_beg
    );
    printf("  description is %.*s\n", desc.count, desc.items);
}

Result extractResults(Tags tags) {
    assert(tags.count > 0);

    Results res = { .items = (Result*)align(tmp, 6), .count = 0 };
    var i = tags.items + 1;
    let end = tags.items[0].descendants_e;
    while(i < end) {
        tryAddResult(res, i);
        i = i->descendants_e;
    }

    return {};
}
