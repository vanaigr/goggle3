#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include<poll.h>
#include<cmath>
#include<GL/glew.h>
#include<GL/glx.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<string.h>
#include<stdbool.h>
#include<algorithm>
#include<chrono>

#include"defs.h"
#include"alloc.h"
#include"text.h"

namespace chrono = std::chrono;

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
            glClear(GL_COLOR_BUFFER_BIT);


            int colors[]{
                0xffffff,
                0xdfdfdf,
                0xafafaf,
                0x8f8f8f,
                0x404040,
            };
            var fd = FontDef{ 24, false,  };
            var x = width / 2;
            var y = height / 2;
            for(int a = 0; a < 5; a++) {
                fd.color = colors[a];
                fd.bold = a == 2;
                text_draw(text, text_c, fd, x, y, width);
                x += 40;
                y -= 25;
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
            if(next_redraw < now && now - next_redraw >= frame_time) {
                next_redraw = now;
            }
            changed = false;
        }

        if(changed && XPending(display) == 0) {
            struct pollfd pfd = {
                .fd = ConnectionNumber(display),
                .events = POLLIN,
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
