#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include <array>
#include<poll.h>
#include<cmath>
#include<GL/glew.h>
#include<GL/glx.h>
#include<curl/curl.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<string.h>
#include<stdbool.h>
#include<algorithm>
#include<chrono>
#include<signal.h>

#include<spawn.h>

#include"defs.h"
#include"alloc.h"

extern char **environ;

namespace chrono = std::chrono;


struct Response {
    char *data;
    int len;
};

static let set_cookie = STR("set-cookie");

static void header_check(char const *buffer, int count) {
    var c_cur = set_cookie.items;
    let c_end = c_cur + set_cookie.count;

    var cur = buffer;
    let end = cur + count;
    if(count < set_cookie.count + 1) return;

    while(c_cur < c_end) {
        var c = *cur;
        let cc = *c_cur;
        if(c >= 'A' && c <= 'Z') c = (c - 'A') + 'a';
        if(c != cc) return;
        cur++;
        c_cur++;
    }

    if(*cur != ':') return;
    cur++;
    while(cur < end && *cur == ' ') cur++;

    let newCookie = parseOneCookie(cur, end);
    if(newCookie.name.count == 0) return;
    /* printf(
        "new cookie: %.*s %.*s",
        newCookie.name.count,
        newCookie.name.items,
        newCookie.value.count,
        newCookie.value.items
    ); */

    var c = fopen("./cookie.txt", "r+");
    var res = ([&]{
        if(c) {
            fseek(c, 0, SEEK_END);
            let c_count = (int)ftell(c);
            fseek(c, 0, SEEK_SET);

            let buf = alloc(c_count);
            fread(buf, c_count, 1, c);

            return getCookies(buf, c_count);
        }
        else {
            // race condition
            c = fopen("./cookie.txt", "w");
            return Cookies{ (Cookie*)align(tmp, 6), 0 };
        }
    })();

    var i = 0;
    for(; i < res.count; i++) {
        let a = res.items[i];
        if(streq(a.name, newCookie.name)) {
            break;
        }
    }
    res.items[i] = newCookie;
    if(i == res.count) {
        res.count++;
        tmp = (char*)(res.items + res.count);
    }

    let updated = encodeCookies(res);

    fseek(c, 0, SEEK_SET);
    fwrite(updated.items, updated.count, 1, c);
    ftruncate(fileno(c), updated.count);
    fclose(c);
}

static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    let ptmp = tmp;
    header_check(buffer, (int)nitems);
    tmp = ptmp;
    return nitems;
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, Response *userp) {
    let chunk_len = (int)(size * nmemb);
    var resp = *userp;

    if(resp.len == 0) {
        let new_len = chunk_len + 1;
        resp.data = (char*)malloc(new_len);
        memcpy(resp.data, contents, chunk_len);
        resp.data[chunk_len] = 0;
        resp.len = new_len;
    }
    else {
        let new_len = (resp.len - 1) + chunk_len + 1;
        resp.data = (char*)realloc(resp.data, new_len);
        memcpy(resp.data + resp.len - 1, contents, chunk_len);
        resp.data[resp.len - 1 + chunk_len] = 0;
        resp.len = new_len;
    }

    *userp = resp;

    return chunk_len;
}

static char *tmp_cookie() {
    let cookie = STR("Cookie: ");
    memcpy(tmp, cookie.items, cookie.count);

    let cookieF = fopen("cookie.txt", "r");
    if(!cookieF) return nullptr;

    fseek(cookieF, 0, SEEK_END);
    let cookieLen = ftell(cookieF);
    fseek(cookieF, 0, SEEK_SET);

    var cur = tmp + cookie.count;
    let end = cur + cookieLen;
    fread(cur, cookieLen, 1, cookieF);
    fclose(cookieF);
    while(cur < end) {
        let c = *cur;
        if(c == '\r' || c == '\n') break;
        cur++;
    }
    *cur = '\0';

    return tmp;
}

static let open_display_keys = std::array{
    STR("·"), STR("S"), STR("J"), STR("K"), STR("D"), STR("L"),
    STR("N"), STR("E"), STR("W"), STR("O"),
    STR("M"), STR("U"), STR("V"), STR("A"), STR("Q"), STR("Z")
};
static let open_hotkeys = std::array{
    STR(" "), STR("s"), STR("j"), STR("k"), STR("d"), STR("l"),
    STR("n"), STR("e"), STR("w"), STR("o"),
    STR("m"), STR("u"), STR("v"), STR("a"), STR("q"), STR("z")
};

static int width;
static int height;
static constexpr let gap = 20;
static int cols;
static int item_w;

struct CalculatedText {
    DrawList url;
    DrawList key;
    DrawList title;
    DrawList desc;
    int favicon_x_off;
    int favicon_y_off;
};

struct Target {
    int count;
    PResult *results;
    CalculatedText *texts;
    int *rowHeights;
};

static Display *display;
static Window window;
static GLBuffer iconsBuf;

#define READ_FILE 0

static Target calculateTarget(Response resp) {
    #if not(READ_FILE)
    let file = fopen("response.html", "w");
    fwrite(resp.data, resp.len > 0 ? resp.len - 1 : 0, 1, file);
    fclose(file);
    #endif

    let results = processResults(
        extractResults(htmlToTags(resp.data, resp.len - 1)),
        iconsBuf
    );
    ce;

    let calculatedTexts = talloc<CalculatedText>(results.count);

    let rc = results.count / cols + 1;
    let calculatedRowHeights = talloc<int>(rc);
    memset(calculatedRowHeights, 0, sizeof(int) * rc);

    var row = 0;
    var col = 1;
    for(var i = 0; i < results.count; i++) {
        let &res = results.items[i];

        let urlStr = FormattedStr{
            .bold = false,
            .italic = false,
            .len = res.site_display_url.count,
            .str = res.site_display_url.items,
            .next = nullptr,
        };
        let url = prepare(&urlStr, { 14, item_w, 0, -14, 22 });
        let urlOff = url.stop_y - 2;

        TextLayout key{};
        var offX = 0;
        var offY = -28;
        if(i < open_display_keys.size()) {
            let keyStr = FormattedStr{
                .bold = false,
                .italic = false,
                .len = open_display_keys[i].count,
                .str = open_display_keys[i].items,
                .next = nullptr
            };
            key = prepare(&keyStr, { 26, item_w, 0, urlOff - 26, 26 });
            // hope it's one line. Otherwise we need to provide a begin_y
            // for the next call, and the default value for it can't be easily made.
            offX = key.stop_x + 7;
            offY = key.stop_y;
        }

        let nameStr = FormattedStr{
            .bold = false,
            .italic = false,
            .len = res.title.count,
            .str = res.title.items,
            .next = nullptr,
        };
        let title_w = res.texture.offset == -1 ? item_w : item_w - res.texture.width - 4;

        let title_font_size = 20;
        let favicon_x_off = item_w - res.texture.width - 2;
        let favicon_y_off = offY + (title_font_size / 2 - 3) + (res.texture.height / 2);

        let title = prepare(&nameStr, { title_font_size, title_w, offX, offY, 26 });
        let titleOff = title.stop_y - 8;

        let desc = prepare(res.desc, { 14, item_w, 0, titleOff - 14, 22 });

        let t = calculatedTexts[i] = {
            .url = url.dl,
            .key = key.dl,
            .title = title.dl,
            .desc = desc.dl,
            .favicon_x_off = favicon_x_off,
            .favicon_y_off = favicon_y_off,
        };
        let totalHeight = -desc.stop_y;

        calculatedRowHeights[row] = std::max(calculatedRowHeights[row], totalHeight);

        col++;
        if(col == cols) {
            row++;
            col = 0;
        }
    }

    ce;
    return { results.count, results.items, calculatedTexts, calculatedRowHeights };
}

static size_t ignore(void *contents, size_t size, size_t nmemb, Response *userp) {
    return size * nmemb;
}

int main(int argc, char **argv) {
    display = XOpenDisplay(NULL);
    if (display == NULL) return 1;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    let multi_handle = curl_multi_init();

    int screen = DefaultScreen(display);

    int screen_width = XDisplayWidth(display, screen);
    int screen_height = XDisplayHeight(display, screen);

    int bot = 20;
    static int pad = std::min(screen_width, screen_height) / 50;
    width = screen_width - 2*pad;
    height = screen_height - 2*pad - bot;

    let rootWindow = RootWindow(display, screen);
    window = XCreateSimpleWindow(
        display, rootWindow,
        pad, pad, width, height,
        0, BlackPixel(display, screen), WhitePixel(display, screen)
    );

    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);

    Atom wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_window_type_splash = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(
        display,
        window,
        wm_window_type,
        XA_ATOM,
        32,
        PropModeReplace,
        (unsigned char *)&wm_window_type_splash,
        1
    );

    XSelectInput(display, window, ExposureMask | KeyPressMask);

    GLint attributeList[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo *visualInfo = glXChooseVisual(display, 0, attributeList);
    GLXContext c = glXCreateContext(display, visualInfo, NULL, GL_TRUE);
    glXMakeCurrent(display, window, c);

    if(glewInit()) {
        printf("glew?\n");
        return 1;
    }

    if(text_init()) {
        return 1;
    }
    ce;

    GLuint fb;
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    ce;

    GLuint fb_tex;
    glGenTextures(1, &fb_tex);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, screen_width, screen_height);
    ce;

    text_bind_texture(fb_tex);
    ce;
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fb_tex, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Framebuffer is not complete\n");
        return 1;
    }
    ce;

    glGenBuffers(1, (GLuint*)&iconsBuf.buffer);

    if(image_init(&iconsBuf)) {
        return 1;
    }
    ce;

    glEnable(GL_BLEND);
#if LCD
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
#else
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

    cols = std::max((width - gap) / (400 + gap), 1);
    item_w = (width - (cols + 1) * gap) / cols;

    let xim = XOpenIM(display, NULL, NULL, NULL);
    let xic = XCreateIC(
        xim,
        XNInputStyle,
        XIMPreeditNothing | XIMStatusNothing,
        XNClientWindow,
        rootWindow,
        NULL
    );
    if(!xic) {
        fprintf(stderr, "Cannot create input context (XIC)\n");
        XCloseIM(xim);
        XCloseDisplay(display);
        return 1;
    }

    XSelectInput(display, rootWindow, KeyPressMask);

    var inserting = true;

    static var wasHidden = false;
    static var hidden = false;

    let handler = [](int signal) {
        if(!hidden) return;
        // This all is not technicallly allowed?

        // NO TMP! This is called inside of signal handler
        // This can allocate memory. Hope we interrupted an allocation
        // in the main program...
        // This opens the window in the current workspace for some reason.
        // So we don't have to do anything.
        XMapWindow(display, window);
        XMoveWindow(display, window, pad, pad);
        hidden = false;
    };

    signal(SIGUSR1, handler);

    XMapWindow(display, window);
    // i3 positions the window with weird top padding.
    // And uses the right the coordinates only if the window is
    // repositioned after the window is shown.
    XMoveWindow(display, window, pad, pad);

    let text = tmp;
    let text_end = tmp + 100'000;
    var text_c = 0;
    var cursor_c = 0;
    tmp += 100'000;

    var response = Response{};

    enum ResponseStatus {
        done,
        processing,
    };
#if READ_FILE
    var responseStatus = processing;
#else
    var responseStatus = done;
#endif
    var req_headers = (curl_slist*){};
    var request = (CURL *){};

    var targetAllocBegin = tmp;
    var targetBuffer = Response{};
    var target = Target{};

    let regularColor = 0xfff582;
    let insertColor = 0x808080;
    let didntOpenColor = 0xff0000;

    var next_redraw = chrono::steady_clock::now();
    let frame_time = static_cast<chrono::microseconds>(chrono::seconds(1)) / 40;

    let repeat_time = static_cast<chrono::microseconds>(chrono::microseconds(100));
    let offAmount = 10;

    var yOff = 0;

    var wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    bool changed = true;
    while (1) {
        if(wasHidden && !hidden) {
            inserting = true;
        }
        wasHidden = hidden;

        let frame_start = chrono::steady_clock::now();

        if(changed) {// && frame_start >= next_redraw) {
            glClearColor(0.12, 0.12, 0.12, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            var x = gap + item_w + gap;
            let initY = height - gap + yOff;
            var y = initY;

            let ptmp = tmp;

            TextLayout res;
            int cursor_off_x;
            int cursor_off_y;
            {
                let params = LayoutParams{ 14, item_w, 0, -14, 20, false };
                let str = FormattedStr{
                    .bold = false,
                    .italic = false,
                    .len = text_c,
                    .str = text,
                    .next = nullptr,
                };
                res = prepare(&str, params);

                let c_str = FormattedStr{
                    .bold = false,
                    .italic = false,
                    .len = cursor_c,
                    .str = text,
                    .next = nullptr,
                };
                let c_res = prepare(&c_str, params);
                cursor_off_x = c_res.stop_x;
                cursor_off_y = c_res.stop_y;
            }
            let prompt_height = -res.stop_y + 5;

            if(inserting) {
                let bot = initY - prompt_height;
                let pad = 4;
                let thk = 2;
                rect(
                    gap - (pad + thk),
                    bot - (pad + thk),
                    item_w + (pad + thk) * 2,
                    prompt_height + (pad + thk) * 2,
                    0xfff2f220
                );
                rect(
                    gap - pad,
                    bot - pad,
                    item_w + pad * 2,
                    prompt_height + pad * 2,
                    { 0.12, 0.12, 0.12, 1 }
                );
            }

            rect(
                gap + cursor_off_x,
                initY + cursor_off_y - 4,
                2,
                18,
                0xffffffff
            );

            if(responseStatus == processing) {
                rect(0, height - 3, width, 3, 0xff27ff48);
            }

            var row = 0;
            var col = 1;
            for(var i = 0; i < target.count; i++) {
                let t = target.texts[i];

                draw(t.url, 0xbdc1c6, x, y);
                draw(t.key, inserting ? insertColor : regularColor, x, y);
                draw(t.title, 0x99c3ff, x, y);
                draw(t.desc, 0xdddee1, x, y);

                let tt = target.results[i].texture;
                if(tt.offset != -1) {
                    let td = TextureDesc{
                        tt.offset,
                        x + t.favicon_x_off,
                        y + t.favicon_y_off,
                        tt.width, tt.height
                    };

                    image_draw({ &td, 1 });
                }

                //rect(x, y - target.rowHeights[row], item_w, target.rowHeights[row]);

                x += item_w + gap;

                col++;
                if(col == cols) {
                    var rh = target.rowHeights[row];
                    if(row == 0) rh = std::max(prompt_height, rh);
                    y -= rh + gap * 2;
                    row++;
                    col = 0;
                    x = gap;
                }
            }

            draw(res.dl, 0xffffff, gap, initY);
            tmp = ptmp;

            // what is even the point of BlitNamed if I must unbind
            // the framebuffer before using it??
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBlitNamedFramebuffer(
                fb, 0,
                0, 0, width, height,
                0, 0, width, height,
                GL_COLOR_BUFFER_BIT, GL_NEAREST
            );
            glBindFramebuffer(GL_FRAMEBUFFER, fb);
            ce;

            glXSwapBuffers(display, window);
            ce;

            next_redraw += frame_time;
            if(next_redraw < frame_start && frame_start - next_redraw >= frame_time) {
                next_redraw = frame_start;
            }
            changed = false;
        }

        while(true) {
            if(responseStatus == processing) {
                let aa = chrono::steady_clock::now();
            #if READ_FILE
                let file = fopen("response.html", "r");
                fseek(file, 0, SEEK_END);
                let size = (int)ftell(file);
                fseek(file, 0, SEEK_SET);
                let buf = (char*)malloc(size + 1);
                fread(buf, size, 1, file);
                fclose(file);
                buf[size] = 0;
                response = { buf, size + 1 };
                responseStatus = done;
            #else
                int left_running;
                curl_multi_perform(multi_handle, &left_running);
                responseStatus = left_running > 0 ? processing : done;
            #endif
                let bb = chrono::steady_clock::now();
                printf(
                    "Get result: %.2f\n",
                    chrono::duration_cast<chrono::microseconds>(bb - aa).count() * 0.001
                );

                if(responseStatus == done) {
                    // In case of an error (e.g. with url having spaces)
                    // we don't receive anything but complete without errors.
                    // Maybe it does set a flag somewhere, but how do I know?
                    if(response.data != nullptr) {
                        tmp = targetAllocBegin;

                        free(targetBuffer.data);
                        targetBuffer = response;
                        response = {};

                let aa = chrono::steady_clock::now();
                        target = calculateTarget(targetBuffer);
                        changed = true;
                let bb = chrono::steady_clock::now();
                printf(
                    "Process: %.2f\n",
                    chrono::duration_cast<chrono::microseconds>(bb - aa).count() * 0.001
                );
                    }
                }
            }

            while(XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.xclient.data.l[0] == wm_delete_window) {
                    goto exit;
                }
                else if (event.type == Expose) {
                    changed = true;
                }
                else if (event.xselection.selection == clipboard) {
                    if (event.xselection.property != None) {
                        Atom type;
                        int format;
                        unsigned long len;
                        unsigned long leftover;
                        unsigned char *data = NULL;

                        XGetWindowProperty(
                            display,
                            window,
                            event.xselection.property,
                            0,
                            (~0L),
                            False,
                            AnyPropertyType,
                            &type,
                            &format,
                            &len,
                            &leftover,
                            &data
                        );

                        if (data) {
                            memmove(
                                text + cursor_c + len,
                                text + cursor_c,
                                text_c - cursor_c
                            );
                            memcpy(text + cursor_c, data, len);
                            cursor_c += len;
                            text_c += len;
                            XFree(data);
                            changed = true;
                        }
                    }
                    break;
                }
                else if (event.type == KeyPress) {
                    let &ev = *(XKeyEvent*)&event;
                    //printf("%d\n", event.xkey.keycode);
                    if(event.xkey.keycode == 55 && (ev.state & ControlMask)) {
                        XConvertSelection(
                            display,
                            clipboard,
                            utf8_string,
                            clipboard,
                            window,
                            CurrentTime
                        );
                        XFlush(display);
                    }
                    else if(event.xkey.keycode == 111) {
                        // up arrow
                        // TODO: ideally it wouldn't depend on repeat rate
                        // and the initial wait period. But it requires XInput2 it seems.
                        yOff -= 20;
                    }
                    else if(event.xkey.keycode == 116) {
                        // down arrow
                        yOff += 20;
                    }
                    else if(event.xkey.keycode == 9) {
                        // escape
                        inserting = false;
                    }
                    else if(event.xkey.keycode == 36) {
                        // enter
                        inserting = false;

                        if(request) {
                            curl_multi_remove_handle(multi_handle, request);
                            curl_easy_cleanup(request);
                            curl_slist_free_all(req_headers);
                        }
                        free(response.data);
                        response = {};

                        request = curl_easy_init();

                        let headers = curl_slist_append(nullptr, "Accept: */*");
                        // It used to be fine, but now it fails without useragent.
                        // And it has to be "valid"...
                        curl_slist_append(headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/132.0.0.0 Safari/537.36");

                        let tc = tmp_cookie();
                        if(tc) curl_slist_append(headers, tc);

                        curl_easy_setopt(request, CURLOPT_HTTPHEADER, headers);
                        curl_easy_setopt(request, CURLOPT_HEADERFUNCTION, header_callback);
                        curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, write_callback);
                        curl_easy_setopt(request, CURLOPT_VERBOSE, 1L);

                        // See SEARXNG engines google
                        let url = STR(
                            "https://www.google.com/search?"
                            "asearch=arc&async=use_ac:true,_fmt:prog&q="
                        );

                        memcpy(tmp, url.items, url.count);

                        let hex = "01234567890ABCDEF";

                        var out = tmp + url.count;
                        for(var i = 0; i < text_c; i++) {
                            let c = text[i];
                            if(c == ' ') {
                                *out++ = '+';
                            }
                            else if(
                                (c >= 'a' && c <= 'z')
                                || (c >= 'A' && c <= 'Z')
                                || (c >= '0' && c <= '9')
                            ) {
                                *out++ = c;
                            }
                            else {
                                *out++ = '%';
                                *out++ = hex[((unsigned char)c >> 4) & 0xf];
                                *out++ = hex[(unsigned char)c & 0xf];
                            }
                        }
                        *out = '\0';

                        curl_easy_setopt(request, CURLOPT_URL, tmp);
                        curl_easy_setopt(request, CURLOPT_WRITEDATA, (void *)&response);

                        curl_multi_add_handle(multi_handle, request);

                        responseStatus = processing;
                    }
                    else if(inserting) {
                        if(event.xkey.keycode == 24 && (ev.state & Mod1Mask)) {
                            // q
                            hidden = true;
                            // race condition...
                            XUnmapWindow(display, window);
                        }
                        else if(event.xkey.keycode == 113) {
                            // left arrow
                            if(cursor_c > 0) {
                                if(ev.state & (ControlMask | Mod1Mask)) {
                                    var cur = text + cursor_c - 1;
                                    while(cur >= text && *cur == ' ') cur--;
                                    while(cur >= text && *cur != ' ') cur--;
                                    cursor_c = cur + 1 - text;
                                }
                                else {
                                    cursor_c--;
                                }
                            }
                        }
                        else if(event.xkey.keycode == 114) {
                            // right arrow
                            if(cursor_c < text_c) {
                                if(ev.state & (ControlMask | Mod1Mask)) {
                                    var cur = text + cursor_c;
                                    let end = text + text_c;
                                    while(cur < end && *cur == ' ') cur++;
                                    while(cur < end && *cur != ' ') cur++;
                                    cursor_c = cur - text;
                                }
                                else {
                                    cursor_c++;
                                }
                            }
                        }
                        else if(event.xkey.keycode == 22) {
                            // backspace
                            if(ev.state & Mod1Mask) {
                                text_c = 0;
                                cursor_c = 0;
                            }
                            else if(ev.state & ControlMask) {
                                // not the best algorithm. but works on unicode.
                                if(cursor_c > 0) {
                                    var cur = text + cursor_c - 1;
                                    while(cur >= text && *cur == ' ') cur--;
                                    while(cur >= text && *cur != ' ') cur--;
                                    let new_cursor_c = (cur + 1 - text);
                                    memmove(
                                        text + new_cursor_c,
                                        text + cursor_c,
                                        text_c - cursor_c
                                    );
                                    let diff = cursor_c - new_cursor_c;
                                    cursor_c = new_cursor_c;
                                    text_c -= diff;
                                }
                            }
                            else {
                                if(cursor_c > 0) {
                                    var cur = text + cursor_c - 1;
                                    while(cur > text && (*cur & 0xc0) == 0x80) cur--;

                                    let new_cursor_c = cur - text;
                                    let diff = cursor_c - new_cursor_c;
                                    memmove(
                                        cur,
                                        text + cursor_c,
                                        text_c - cursor_c
                                    );
                                    cursor_c = new_cursor_c;
                                    text_c -= diff;
                                }
                            }
                        }
                        else {
                            KeySym keysym;
                            Status compose;

                            int len = Xutf8LookupString(
                                xic,
                                &event.xkey,
                                tmp,
                                text_end - text,
                                &keysym,
                                &compose
                            );
                            memmove(
                                text + cursor_c + len,
                                text + cursor_c,
                                text_c - cursor_c
                            );
                            memcpy(text + cursor_c, tmp, len);
                            cursor_c += len;
                            text_c += len;
                        }
                    }
                    else {
                        if(event.xkey.keycode == 24) {
                            // q
                            hidden = true;
                            // race condition...
                            XUnmapWindow(display, window);
                        }
                        if(event.xkey.keycode == 31) {
                            // i
                            inserting = true;
                        }
                        else {
                            KeySym keysym;
                            XComposeStatus compose;
                            int len = XLookupString(
                                &event.xkey,
                                tmp,
                                tmp_end - tmp,
                                &keysym,
                                &compose
                            );
                            let typed = str{ tmp, len };

                            for(var i = 0; i < target.count; i++) {
                                if(streq(typed, open_hotkeys[i])) {
                                    if(open_url(target.results[i].url)) {
                                        hidden = true;
                                        // race condition...
                                        XUnmapWindow(display, window);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    changed = true;
                }
            }

            int timeout;
            if(changed) {
                let now = chrono::steady_clock::now();
                let d = next_redraw - now;
                timeout = chrono::duration_cast<chrono::milliseconds>(d).count();
                if(timeout <= 0) {
                    break;
                }
            }
            else {
                timeout = -1;
            }

            let wait_for_x11 = XPending(display) == 0;
            let wait_for_curl = responseStatus == processing;
            if((wait_for_curl && wait_for_x11) || !wait_for_curl) {
                curl_waitfd pfd[1];
                int pfd_count = 0;
                if(wait_for_x11) {
                    pfd[pfd_count++] = {
                        .fd = ConnectionNumber(display),
                        .events = POLLIN,
                        .revents = 0,
                    };
                }

                // TODO: loop until number of changed descriptors is > 0
                curl_multi_wait(multi_handle, pfd, pfd_count, 1000, nullptr);
            }
        }
    }

    exit:

    if(request) curl_multi_remove_handle(multi_handle, request);
    curl_easy_cleanup(request);
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();

    XDestroyWindow(display, window);
    XCloseDisplay(display);
}
