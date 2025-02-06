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
};

struct Target {
    int count;
    PResult *results;
    CalculatedText *texts;
    int *rowHeights;
};

static Display *display;
static Window window;

#define READ_FILE 0

static Target calculateTarget(Response resp) {
    #if not(READ_FILE)
    let file = fopen("response.html", "w");
    fwrite(resp.data, resp.len, 1, file);
    fclose(file);
    #endif

    let results = processResults(extractResults(htmlToTags(resp.data, resp.len - 1)));

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
        let title = prepare(&nameStr, { 20, item_w, offX, offY, 26 });
        let titleOff = title.stop_y - 8;

        let desc = prepare(res.desc, { 14, item_w, 0, titleOff - 14, 22 });

        let t = calculatedTexts[i] = {
            .url = url.dl,
            .key = key.dl,
            .title = title.dl,
            .desc = desc.dl,
        };
        let totalHeight = -desc.stop_y;

        calculatedRowHeights[row] = std::max(calculatedRowHeights[row], totalHeight);

        col++;
        if(col == cols) {
            row++;
            col = 0;
        }
    }

    return { results.count, results.items, calculatedTexts, calculatedRowHeights };
}

int main(int argc, char **argv) {
    display = XOpenDisplay(NULL);
    if (display == NULL) return 1;

    let headers = curl_slist_append(nullptr, "Accept: */*");
    // It used to be fine, but now it fails without useragent.
    // And it has to be "valid"...
    curl_slist_append(headers, "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/132.0.0.0 Safari/537.36");

    let cookie = STR("Cookie: ");
    memcpy(tmp, cookie.items, cookie.count);
    let cookieF = fopen("cookie.txt", "r");
    fseek(cookieF, 0, SEEK_END);
    let cookieLen = ftell(cookieF);
    fseek(cookieF, 0, SEEK_SET);
    var cur = tmp + cookie.count;
    let end = cur + cookieLen;
    fread(cur, cookieLen, 1, cookieF);
    while(cur < end) {
        let c = *cur;
        if(c == '\r' || c == '\n') break;
        cur++;
    }
    *cur = '\0';

    curl_slist_append(headers, tmp);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    let multi_handle = curl_multi_init();

    int screen = DefaultScreen(display);

    int screen_width = XDisplayWidth(display, screen);
    int screen_height = XDisplayHeight(display, screen);

    int bot = 20;
    static int pad = std::min(screen_width, screen_height) / 30;
    width = screen_width - 2*pad;
    height = screen_height - 2*pad - bot;

    let rootWindow = RootWindow(display, screen);
    window = XCreateSimpleWindow(
        display, rootWindow,
        pad, pad, width, height,
        0, BlackPixel(display, screen), WhitePixel(display, screen)
    );

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

    var inserting = false;

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
    };

    signal(SIGUSR1, handler);

    XMapWindow(display, window);
    // i3 positions the window with weird top padding.
    // And uses the right the coordinates only if the window is
    // repositioned after the window is shown.
    XMoveWindow(display, window, pad, pad);

    let text = tmp;
    let text_end = tmp + 4096;
    var text_c = 0;
    tmp += 4096;

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
        let frame_start = chrono::steady_clock::now();

        if(changed && frame_start >= next_redraw) {
            glClearColor(
                0.12,
                0.12,
                0.12,
                1
            );
            glClear(GL_COLOR_BUFFER_BIT);

            var x = gap + item_w + gap;
            let initY = height - gap + yOff;
            var y = initY;

            if(responseStatus == processing) {
                rect(
                    0,
                    height - 3,
                    width,
                    3,
                    0xff27ff48
                );
            }

            var row = 0;
            var col = 1;
            for(var i = 0; i < target.count; i++) {
                let t = target.texts[i];

                draw(t.url, 0xbdc1c6, x, y);
                draw(t.key, inserting ? insertColor : regularColor, x, y);
                draw(t.title, 0x99c3ff, x, y);
                draw(t.desc, 0xdddee1, x, y);

                //rect(x, y - target.rowHeights[row], item_w, target.rowHeights[row]);

                x += item_w + gap;

                col++;
                if(col == cols) {
                    y -= target.rowHeights[row] + gap * 2;
                    row++;
                    col = 0;
                    x = gap;
                }
            }

            {
                let ptmp = tmp;
                let str = FormattedStr{
                    .bold = false,
                    .italic = false,
                    .len = text_c,
                    .str = text,
                    .next = nullptr,
                };
                let res = prepare(&str, { 14, item_w, 0, -14, false });
                draw(res.dl, 0xffffff, gap, initY);
                tmp = ptmp;
            }

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
                buf[size] = 0;
                response = { size, buf, size + 1, false };
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
                else if (event.type == KeyPress) {
                    if(event.xkey.keycode == 111) {
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
                    else if(inserting) {
                        if(event.xkey.keycode == 22) {
                            // backspace
                            text_c = text_c >= 1 ? text_c - 1 : 0;
                        }
                        else if(event.xkey.keycode == 36) {
                            // enter
                            inserting = false;

                            if(request) {
                                curl_multi_remove_handle(multi_handle, request);
                                curl_easy_cleanup(request);
                            }
                            free(response.data);
                            response = {};

                            request = curl_easy_init();

                            curl_easy_setopt(request, CURLOPT_HTTPHEADER, headers);
                            curl_easy_setopt(request, CURLOPT_WRITEFUNCTION, write_callback);
                            curl_easy_setopt(request, CURLOPT_VERBOSE, 1L);

                            let url = STR("https://www.google.com/search?gbv=1&q=");

                            memcpy(tmp, url.items, url.count);

                            let escaped = STR(":/?#[]@!$&'()*+,;=");
                            let hex = "01234567890ABCDEF";
                            let eend = escaped.items + escaped.count;

                            var out = tmp + url.count;
                            for(var i = 0; i < text_c; i++) {
                                let c = text[i];
                                let it = find(
                                    escaped.items,
                                    eend,
                                    c
                                );
                                if(it != eend) {
                                    *out++ = '%';
                                    *out++ = hex[c & 0xf];
                                    *out++ = hex[(c >> 4) & 0xf];
                                }
                                else if(c == ' ') {
                                    *out++ = '+';
                                }
                                else {
                                    *out++ = c;
                                }
                            }
                            *out = '\0';

                            curl_easy_setopt(request, CURLOPT_URL, tmp);
                            curl_easy_setopt(request, CURLOPT_WRITEDATA, (void *)&response);

                            curl_multi_add_handle(multi_handle, request);

                            responseStatus = processing;
                        }
                        else {
                            KeySym keysym;
                            Status compose;

                            int len = Xutf8LookupString(
                                xic,
                                &event.xkey,
                                text + text_c,
                                text_end - text,
                                &keysym,
                                &compose
                            );
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
