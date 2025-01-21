#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include<ft2build.h>
#include FT_FREETYPE_H
#include<GL/glew.h>
#include<GL/glx.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<string.h>
#include<stdbool.h>
#include<algorithm>

FT_Library F;

#define let auto const
#define var auto

static constexpr int tmp_max_c = 200000;
static uint8_t tmp[tmp_max_c];
static int tmp_c;

using Error = int;

void shaderReport(GLint shader, char const *name = "whatever") {
    GLint res;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
    if(!res) {
        let beg = tmp + tmp_c;
        GLint len;
        glGetShaderInfoLog(shader, tmp_max_c - tmp_c, &len, (char*)beg);
        printf("%s shader said %.*s", name, len, beg);
        throw 1;
    }
}

void programReport(GLint prog, char const *name = "whatever") {
    GLint res;
    glGetProgramiv(prog, GL_LINK_STATUS, &res);
    if(!res) {
        let beg = tmp + tmp_c;
        GLint len;
        glGetProgramInfoLog(prog, tmp_max_c - tmp_c, &len, (char*)beg);
        printf("%s program said %.*s", name, len, beg);
        throw 1;
    }
}

void check_error(int line) {
    GLint err = glGetError();
    if(err) {
        printf("[%d] What is %d\n", line, err);
        throw 1;
    }
}
#define ce check_error(__LINE__)

typedef struct {
    int advance; // advance width in px
    int texture_x, texture_w;
    int texture_left_off;
    int texture_top_off;
    bool initialized;
} CharInfo;

static CharInfo chars16[200000];


int main(int argc, char **argv) {
    chars16[0] = {
        .advance = 0,
        .texture_x = 0,
        .texture_w = 0,
        .initialized = true,
    };

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

    FT_Init_FreeType(&F);

    FT_Face face;
    FT_New_Face(F, "/usr/share/fonts/truetype/freefont/FreeSans.ttf", 0, &face);
    if(FT_Set_Pixel_Sizes(face, 0, 48)) {
        printf("font where?\n");
        return 1;
    }

    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 1024, 48);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    ce;

    var off = chars16[0].texture_w;
    let get_glyph = [&](int code) -> CharInfo {
        var &ci = chars16[code];
        if(ci.initialized) {
            return ci;
        }

        if(FT_Load_Char(face, code, FT_LOAD_RENDER)) {
            printf("died on %d \n", code);
            ci = chars16[0];
            return ci;
        }

        let g = face->glyph;
        let b = g->bitmap;


        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glTexSubImage2D(
            GL_TEXTURE_2D, 0,
            off, 0, b.width, b.rows,
            GL_RED, GL_UNSIGNED_BYTE,
            b.buffer
        );

        /*for(int i = 0; i < b.rows; i++) {
            for(int j = 0; j < b.width; j++) {
                printf("%c", b.buffer[i * b.width + j] ? '#' : ' ');
            }
            printf("\n");
        }
            printf("\n");*/


        ci = CharInfo{
            .advance = (int)g->linearHoriAdvance,
            .texture_x = off,
            .texture_w = (int)b.width,
            .texture_left_off = g->bitmap_left,
            .texture_top_off = g->bitmap_top,
            .initialized = true,
        };

        off += b.width;

        return ci;
    };

    let prog = glCreateProgram();
    let frag = glCreateShader(GL_FRAGMENT_SHADER);
    let frag_src = R"(
        #version 440
        uniform sampler2D tex;

        flat in int off;
        flat in int w;
        in vec2 uv;

        out vec4 result;

        void main() {
            ivec2 size = textureSize(tex, 0);
            // note: yes, interpolating to w. Because uv 0 and 1 are edges.
            ivec2 coord = ivec2(mix(vec2(off, size.y), vec2(off + w, 0), uv));
            float alpha = texelFetch(tex, coord, 0).r;
            result = vec4(vec3(alpha), 1.0);
        }
    )";
    glShaderSource(frag, 1, &frag_src, nullptr);

    let vert = glCreateShader(GL_VERTEX_SHADER);
    let vert_src = R"(
        #version 440

        in vec2 coord;
        in ivec2 texture;

        out vec2 uv;
        flat out int off;
        flat out int w;

        vec2 points[6] = {
            vec2(0, 0),
            vec2(0, 1),
            vec2(1, 0),
            vec2(0, 1),
            vec2(1, 0),
            vec2(1, 1)
        };

        void main() {
            uv = points[gl_VertexID % 6];
            off = texture[0];
            w = texture[1];
            gl_Position = vec4(coord, 0.0, 1.0);
        }
    )";
    glShaderSource(vert, 1, &vert_src, nullptr);

    glCompileShader(frag);
    glCompileShader(vert);

    shaderReport(vert, "vertex");
    shaderReport(frag, "fragment");

    glAttachShader(prog, frag);
    glAttachShader(prog, vert);
    glLinkProgram(prog);
    programReport(prog);
    ce;

    get_glyph('a');
    get_glyph('b');
    get_glyph('o');
    char text[] = "aboba";
    ce;

    glUseProgram(prog);
    let loc = glGetUniformLocation(prog, "tex");
    glUniform1i(loc, 0);
    ce;

    let coord = glGetAttribLocation(prog, "coord");
    ce;

    let texture0 = glGetAttribLocation(prog, "texture");
    ce;

    GLuint va;
    glCreateVertexArrays(1, &va);
    glBindVertexArray(va);
    ce;

    let width_fac = 2.0 / width;
    let height_fac = 2.0 / height;

    GLuint vb;
    glCreateBuffers(1, &vb);
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    float points[12*5]{};
    float x = 0;
    for(int i = 0; i < 5; i++) {
        let c = text[i];
        let ci = chars16[c];
        let left_off = x + ci.texture_left_off;
        let top_off = 0 + ci.texture_top_off;
        points[i*12 + 0] = left_off * width_fac;
        points[i*12 + 1] = top_off * height_fac;
        points[i*12 + 2] = left_off * width_fac;
        points[i*12 + 3] = (top_off + 48.0f) * height_fac;
        points[i*12 + 4] = (left_off + ci.texture_w) * width_fac;
        points[i*12 + 5] = top_off * height_fac;

        points[i*12 + 6] = left_off * width_fac;
        points[i*12 + 7] = (top_off + 48.0f) * height_fac;
        points[i*12 + 8] = (left_off + ci.texture_w) * width_fac;
        points[i*12 + 9] = top_off * height_fac;
        points[i*12 + 10] = (left_off + ci.texture_w) * width_fac;
        points[i*12 + 11] = (top_off + 48.0f) * height_fac;

        x += ci.advance * (1.0f / 65536);
    }
    glBufferData(GL_ARRAY_BUFFER, 60 * 4, &points, GL_STATIC_DRAW);

    glVertexAttribPointer(coord, 2, GL_FLOAT, false, 8, (void*)0);
    glEnableVertexAttribArray(coord);

    GLuint vb2;
    glCreateBuffers(1, &vb2);
    glBindBuffer(GL_ARRAY_BUFFER, vb2);
    int textures[12*5]{};
    for(int i = 0; i < 5; i++) {
        let c = text[i];
        let ci = chars16[c];
        textures[i*12 + 0] = ci.texture_x;
        textures[i*12 + 2] = ci.texture_x;
        textures[i*12 + 4] = ci.texture_x;
        textures[i*12 + 6] = ci.texture_x;
        textures[i*12 + 8] = ci.texture_x;
        textures[i*12 + 10] = ci.texture_x;

        textures[i*12 + 1] = ci.texture_w;
        textures[i*12 + 3] = ci.texture_w;
        textures[i*12 + 5] = ci.texture_w;
        textures[i*12 + 7] = ci.texture_w;
        textures[i*12 + 9] = ci.texture_w;
        textures[i*12 + 11] = ci.texture_w;
    }
    glBufferData(GL_ARRAY_BUFFER, 60 * 4, &textures, GL_STATIC_DRAW);

    glVertexAttribIPointer(texture0, 2, GL_INT, 8, (void*)0);
    glEnableVertexAttribArray(texture0);

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

        glUseProgram(prog);
        glDrawArrays(GL_TRIANGLES, 0, 6*5);

        glXSwapBuffers(display, window);
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
