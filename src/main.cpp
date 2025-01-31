#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
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

#include"defs.h"
#include"alloc.h"

namespace chrono = std::chrono;


struct Response {
    char *data;
    int len;
};

size_t write_callback(void *contents, size_t size, size_t nmemb, Response *userp) {
    let chunk_len = size * nmemb;
    var resp = *userp;

    let new_len = resp.len + chunk_len;
    resp.data = (char*)realloc(resp.data, new_len);
    memcpy(resp.data + resp.len, contents, chunk_len);
    resp.len = new_len;

    *userp = resp;

    return chunk_len;
}

#if 0
#include<stdio.h>
int main() {
#if 0
    char const url[] = "https://www.google.com/search?asearch=arc&async=use_ac:true,_fmt:prog&q=a";

    let headers = curl_slist_append(nullptr, "Accept: */*");
    // now it fails without useragent. And it has to be "valid"...
    curl_slist_append(headers, "user-agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/132.0.0.0 Safari/537.36");

    CURL *curl;
    CURLM *multi_handle;
    int still_running;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    multi_handle = curl_multi_init();

    if (!curl || !multi_handle) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    Response resp{};
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);


    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_multi_add_handle(multi_handle, curl);

    do {
        int numfds;
        curl_multi_perform(multi_handle, &still_running);

        if (still_running) {
            curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
        }
    } while (still_running);

    // let file = fopen("response.txt", "w");
    // fwrite(resp.data, resp.len, 1, file);
    // fclose(file);

    curl_multi_remove_handle(multi_handle, curl);
    curl_easy_cleanup(curl);
    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();
#else
    let file = fopen("response.txt", "r");
    fseek(file, 0, SEEK_END);
    let len = (int)ftell(file);
    fseek(file, 0, SEEK_SET);
    let buf = alloc(len, 6);
    fread(buf, len, 1, file);
    let resp = Response{ .data = buf, .len = len };
#endif

    let ptmp = tmp;
    let results = processResults(extractResults(htmlToTags(resp.data, resp.len)));

    for(var i = 0; i < results.count; i++) {
        let res = results.items[i];

        printf("Result %d:\n", i);
        printf("  title is %.*s\n", res.title.count, res.title.items);
        printf("  website name is %.*s\n", res.site_name.count, res.site_name.items);
        printf(
            "  website url name is %.*s\n",
            res.site_display_url.count, res.site_display_url.items
        );

        printf("  description is ");
        var dc = res.desc;
        while(dc) {
            printf("[%d%d]|%.*s|", dc->bold, dc->italic, dc->len, dc->str);
            dc = dc->next;
        }
        printf("\n");
    }

    tmp = ptmp;

    return 0;
}
#endif

int main(int argc, char **argv) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) return 1;

    int screen = DefaultScreen(display);

    int screen_width = XDisplayWidth(display, screen);
    int screen_height = XDisplayHeight(display, screen);

    // for i3, y=1 is 21 pixels away from top. Possibly because of tabs?
    // But why can I obscure the bottom bar with workspaces then?
    int what = 20, bot = 20;
    int pad = std::min(screen_width, screen_height) / 30;
    int pad_top = std::max(0, pad - what);
    int width = screen_width - 2*pad;
    int height = screen_height - bot - (what + pad_top) - pad;

    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen),
        pad, pad_top, width, height,
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

    let width_fac = 2.0 / width;
    let height_fac = 2.0 / height;

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

    let file = fopen("response.txt", "r");
    fseek(file, 0, SEEK_END);
    let len = (int)ftell(file);
    fseek(file, 0, SEEK_SET);
    let buf = alloc(len, 6);
    fread(buf, len, 1, file);
    let resp = Response{ .data = buf, .len = len };

    let ptmp = tmp;
    let results = processResults(extractResults(htmlToTags(resp.data, resp.len)));

    let &first = results.items[0];
    let urlStr = FormattedStr{
        .bold = false,
        .italic = false,
        .len = first.site_display_url.count,
        .str = first.site_display_url.items,
        .next = nullptr,
    };
    let url = prepare(&urlStr, 14, 500);

    let nameStr = FormattedStr{
        .bold = false,
        .italic = false,
        .len = first.title.count,
        .str = first.title.items,
        .next = nullptr,
    };
    let title = prepare(&nameStr, 24, 500);

    let desc = prepare(first.desc, 14, 500);

    XMapWindow(display, window);

    let text = tmp;
    let text_end = tmp + 4096;
    var text_c = 5;
    memcpy(text, "aaaaa", 5);
    tmp += 4096;

    var next_redraw = chrono::steady_clock::now();
    let frame_time = static_cast<chrono::microseconds>(chrono::seconds(1)) / 40;

    bool changed = true;
    while (1) {
        let now = chrono::steady_clock::now();
        if(changed && now >= next_redraw) {
            glClearColor(
                0.02,
                0.02,
                0.02,
                1
            );
            glClear(GL_COLOR_BUFFER_BIT);

            var x = width / 2;
            var y = height / 2;
            draw(url.dl, 0xbdc1c6, x, y);
            y += url.stop_y - 30;
            draw(title.dl, 0x99c3ff, x, y);
            y += title.stop_y - 22;
            draw(desc.dl, 0xdddee1, x, y);

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
            if(next_redraw < now && now - next_redraw >= frame_time) {
                next_redraw = now;
            }
            changed = false;
        }

        if(changed && XPending(display) == 0) {
            struct pollfd pfd = {
                .fd = ConnectionNumber(display),
                .events = POLLIN,
                .revents = 0,
            };

            while(true) {
                let now = chrono::steady_clock::now();
                let timeout = chrono::duration_cast<chrono::milliseconds>(
                    next_redraw - now
                ).count();
                if(timeout <= 0 || poll(&pfd, 1, timeout) > 0) {
                    break;
                }
            }
        }

        while(!changed || XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == Expose) {
                changed = true;
            } else if (event.type == KeyPress) {
                if(event.xkey.keycode == 24) {
                    // q
                    goto exit;
                }
                else if(event.xkey.keycode == 22) {
                    // backspace
                    text_c = text_c >= 1 ? text_c - 1 : 0;
                }
                else if(event.xkey.keycode == 36) {
                    // enter
                    text[text_c++] = '\n';
                }
                else {
                    KeySym keysym;
                    XComposeStatus compose;
                    let p = text + text_c;
                    int len = XLookupString(&event.xkey, p, text_end - p, &keysym, &compose);
                    text_c += len;
                }
                changed = true;
            }
        }
    }

    exit:

    XDestroyWindow(display, window);
    XCloseDisplay(display);
}
