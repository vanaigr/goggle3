#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include<ft2build.h>
#include FT_FREETYPE_H
#include<GL/glx.h>
#include<stdlib.h>
#include<stdio.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

FT_Library F;

int main(int argc, char **argv) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) return 1;

    int screen = DefaultScreen(display);

    int screen_width = XDisplayWidth(display, screen);
    int screen_height = XDisplayHeight(display, screen);

    // for i3, y=1 is 21 pixels away from top. Possibly because of tabs?
    // But why can I obscure the bottom bar with workspaces then?
    int what = 20, bot = 20;
    int pad = MIN(screen_width, screen_height) / 30;
    int pad_top = MAX(0, pad - what);
    int height = screen_height - bot - (what + pad_top) - pad;

    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen),
        pad, pad_top, screen_width - 2*pad, height,
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

    FT_Init_FreeType(&F);

    FT_Face face;
    FT_New_Face(F, "/usr/share/fonts/truetype/arial.ttf", 0, &face);

    XMapWindow(display, window);

    XEvent event;
    int i = 0;
    while (1) {
        XNextEvent(display, &event);
        if (event.type == Expose) {
            // Draw something if needed
        } else if (event.type == KeyPress) {
            if(event.xkey.keycode == 24/* q */) {
                break;
            }
        }

        glClearColor(0.5, 0.5, 0.5, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);


        glColor3f(0.0f, 0.0f, 1.0f);

        glXSwapBuffers(display, window);
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
