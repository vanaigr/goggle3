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

    var container = &root;
    while(true) {
        var found = false;
        var pos = container->name.items + container->name.count;
        let end = container->content_end;
        while(pos < end) {
            let cmp = STR("data-rpos");
            if(streq({ pos + 1, std::min<int>(cmp.count, end - (pos + 1)) }, cmp )) {
                found = true;
                break;
            }
            pos = find(pos + 1, end, ' ');
        }

        if(found) break;

        if(container->descendants_e == container + 1) return;
        container = container + 1;
        if(!streq(container->name, STR("div"))) return;
    }

    if(container->descendants_e == container + 1) return;
    container = container + 1;
    if(!streq(container->name, STR("div"))) return;

    if(container->descendants_e == container + 1) return;
    container = container + 1;
    if(!streq(container->name, STR("div"))) return;

    let &cont = *container;

    let &a = *findFirstName(&cont, cont.descendants_e, STR("a"));
    if(&a == cont.descendants_e) return;

    let &titleTag = *findFirstName(&a, a.descendants_e, STR("h3"));
    if(&titleTag == a.descendants_e) return;

    let &iconCont = *findFirstName(&titleTag + 1, a.descendants_e, STR("span"));
    let &websiteNameTag = *findFirstName(&iconCont + 1, a.descendants_e, STR("span"));

    let &cite = *findFirstName(&websiteNameTag + 1, a.descendants_e, STR("cite"));

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
