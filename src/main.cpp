#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
#include <cmath>
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
#include<chrono>

namespace chrono = std::chrono;

#define LCD 1

FT_Library F;

#define let auto const
#define var auto

static constexpr int tmp_max_c = 200000;
static char tmp_buf[tmp_max_c];
static char *tmp = tmp_buf;
static char *tmp_end = tmp_buf + tmp_max_c;

using Error = int;

void shaderReport(GLint shader, char const *name = "whatever") {
    GLint res;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
    if(!res) {
        GLint len;
        glGetShaderInfoLog(shader, tmp_end - tmp, &len, tmp);
        printf("%s shader said %.*s", name, len, tmp);
        fflush(stdout);
        throw 1;
    }
}

void programReport(GLint prog, char const *name = "whatever") {
    GLint res;
    glGetProgramiv(prog, GL_LINK_STATUS, &res);
    if(!res) {
        GLint len;
        glGetProgramInfoLog(prog, tmp_end - tmp, &len, tmp);
        printf("%s program said %.*s", name, len, tmp);
        fflush(stdout);
        throw 1;
    }
}

void check_error(int line) {
    GLint err = glGetError();
    if(err) {
        printf("[%d] What is %d\n", line, err);
        fflush(stdout);
        throw 1;
    }
}
#define ce check_error(__LINE__)

typedef struct {
    int advance; // 26.6
    int texture_x, texture_w;
    int texture_left_off; // 26.6
    int texture_top_off; // 26.6
    int glyph_index;
    bool initialized;
} CharInfo;

static CharInfo chars16[200000];

char *alloc(int size, int align_pow = 0) {
    var res = (((uintptr_t)tmp >> align_pow) << align_pow);
    if(res != (uintptr_t)tmp) res += 1 << align_pow;

    tmp = (char*)res + size;
    return (char*)res;
}

template<typename T>
T *talloc(int count) {
    return (T *)alloc(sizeof(T) * count, 6);
}


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

    int font_size = 16;

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
    FT_New_Face(F, "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf", 0, &face);
    if(FT_Set_Pixel_Sizes(face, 0, font_size)) {
        printf("font where?\n");
        return 1;
    }

    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
#if LCD
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1024, font_size);
#else
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 1024, font_size);
#endif

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    // why????? why can't I just unactivate the texture?
    glActiveTexture(GL_TEXTURE0 + 79);

    ce;

    var off = chars16[0].texture_w;
    let get_glyph = [&](int code) -> CharInfo {
        var &ci = chars16[code];
        if(ci.initialized) {
            return ci;
        }

        int glyph_index = FT_Get_Char_Index(face, code);
        if(FT_Load_Glyph(
            face,
            glyph_index,
            FT_LOAD_RENDER
        #if LCD
                | FT_LOAD_TARGET_LCD
        #endif
        )) {
            printf("died on %d \n", code);
            ci = chars16[0];
            return ci;
        }

        let g = face->glyph;
        let b = g->bitmap;

        int gl_texture_w;

    #if LCD
        gl_texture_w = (int)(b.width / 3);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glTextureSubImage2D(
            texture, 0,
            off, 0, gl_texture_w, b.rows,
            GL_RGB, GL_UNSIGNED_BYTE,
            b.buffer
        );
    #else
        gl_texture_w = (int)b.width;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glTextureSubImage2D(
            texture, 0,
            off, 0, gl_texture_w, b.rows,
            GL_RED, GL_UNSIGNED_BYTE,
            b.buffer
        );
    #endif

        /* char const *chars = "rgb";
        for(int i = 0; i < b.rows; i++) {

            for(int j = 0; j < b.width; j++) {
                if(j % 3 == 0) printf("|");
                printf("%c", b.buffer[i * b.pitch + j] ? chars[j % 3] : ' ');
            }
            printf("\n");
        }
        printf("\n"); */

        // why does this exist, and why is it not correct
        // and crops the character? Shouldn't this be the
        // of pixels in the bitmap horizontally?
        // .width = (int)g->metrics.width,

        ci = CharInfo{
            .advance = (int)g->advance.x,
            .texture_x = off,
            .texture_w = (int)gl_texture_w,
            .texture_left_off = g->bitmap_left,
            .texture_top_off = g->bitmap_top,
            .glyph_index = glyph_index,
            .initialized = true,
        };

        off += b.width;

        return ci;
    };

    let prog = glCreateProgram();
    let compute = glCreateShader(GL_COMPUTE_SHADER);
    let compute_src = R"(
        #version 440

        layout(local_size_x = 64, local_size_y = 1) in;
        layout(rgba32f, binding = 0) coherent uniform image2D outImg;

        layout(rgba32f, binding = 1) readonly uniform image2D glyphs;
        layout(std430, binding = 2) buffer Characters {
            int data[];
        };

        void main() {
            int groupId = int(gl_WorkGroupID.x);

            ivec4 character = ivec4(
                data[groupId * 6 + 0],
                data[groupId * 6 + 1],
                data[groupId * 6 + 2],
                data[groupId * 6 + 3]
            );

            ivec2 texture_off = ivec2(
                data[groupId * 6 + 4],
                data[groupId * 6 + 5]
            );

            int total = character.b * character.a;

            int off = int(gl_LocalInvocationIndex.x);
            for(; off < total; off += 64) {
                int x = off / character.a;
                int y = off % character.a;
                ivec2 coord = character.xy + ivec2(x, y);
                int height = imageSize(glyphs).y;
        )"

    #if LCD
        R"(
                ivec2 glyphs_coord = texture_off + ivec2(x, height-1 - y);
                vec3 alpha = imageLoad(glyphs, glyphs_coord).rgb;

                // hack (more or less). If glyphs intersect, we may overwrite
                // their color with transparent. Ideally should copmareExchange
                // values.
                if(any(greaterThan(alpha, vec3(0)))) {
                    vec4 prev_color = imageLoad(outImg, coord);
                    vec4 color = vec4(mix(prev_color.rgb, vec3(1, 1, 1), alpha), 1);
                    imageStore(outImg, coord, color);
                }
        )"
    #else
        R"(
                ivec2 glyphs_coord = texture_off + ivec2(x, height-1 - y);
                float alpha = imageLoad(glyphs, glyphs_coord).r;

                if(alpha > 0) {
                    vec4 prev_color = imageLoad(outImg, coord);
                    vec4 color = mix(prev_color, vec4(1, 1, 1, 1), alpha);
                    imageStore(outImg, coord, color);
                }
        )"
    #endif
        R"(
            }
        }
    )";

    glShaderSource(compute, 1, &compute_src, nullptr);
    glCompileShader(compute);
    shaderReport(compute, "compute");

    glAttachShader(prog, compute);
    glLinkProgram(prog);
    programReport(prog);
    ce;

    GLuint chars_buf;
    glCreateBuffers(1, &chars_buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chars_buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, chars_buf);
    ce;

    glUseProgram(prog);

    let loc = glGetUniformLocation(prog, "outImg");
    glUniform1i(loc, 0);

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

    glBindImageTexture(0, fb_tex, 0, false, 0, GL_READ_WRITE, GL_RGBA8);
    ce;
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fb_tex, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Framebuffer is not complete\n");
        return 1;
    }
    ce;

    #if LCD
    // no docs, no nothing. Guess yourself that images DON'D SUPPORT RGB8, ONLY RGBA8
    glBindImageTexture(1, texture, 0, false, 0, GL_READ_ONLY, GL_RGBA8);
    #else
    glBindImageTexture(1, texture, 0, false, 0, GL_READ_ONLY, GL_R8);
    #endif
    ce;

    glEnable(GL_BLEND);
#if LCD
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
#else
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

    XMapWindow(display, window);

    bool keming = true;

    let text = tmp;
    let text_end = tmp + 4096;
    var text_c = 5;
    memcpy(text, "aaaaa", 5);
    tmp += 4096;

    var next_redraw = chrono::steady_clock::now();
    let frame_time = static_cast<chrono::microseconds>(chrono::seconds(1)) / 40;

    XEvent event;
    while (1) {
        XNextEvent(display, &event);
        if (event.type == Expose) {
            // Draw something if needed
        } else if (event.type == KeyPress) {
            if(event.xkey.keycode == 24) {
                // q
                break;
            }
            else if(event.xkey.keycode == 64) {
                // left alt
                keming = !keming;
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
                printf("%d is %.*s\n", event.xkey.keycode, len, p);
                text_c += len;
            }
        }

        let now = chrono::steady_clock::now();
        if(now >= next_redraw) {
            let ptmp = tmp;

            let data = talloc<int32_t>(text_c * 6);

            let o6 = 1.0f / 64;

            float x = width / 2.0f;
            float y = height / 2.0f;
            for(var i = 0; i < text_c; i++) {
                let c = text[i];
                let ci = get_glyph(c);
                if(c == '\n') {
                    x = 0;
                    y -= font_size * 1.25;
                    continue;
                }

                if(i != 0 && keming) {
                    FT_Vector kerning;
                    let error = FT_Get_Kerning(
                        face,
                        chars16[text[i-1]].glyph_index,
                        ci.glyph_index,
                        FT_KERNING_DEFAULT,
                        &kerning
                    );
                    x += kerning.x * o6;
                }

                var left_off = x + ci.texture_left_off;
                var top_off = y + ci.texture_top_off;

                let gw = ci.texture_w;
                let gh = font_size;

                data[i * 6 + 0] = int{ (int)std::floor(left_off) };
                data[i * 6 + 1] = int{ (int)std::floor(top_off) };
                data[i * 6 + 2] = int{ gw };
                data[i * 6 + 3] = int{ gh };
                data[i * 6 + 4] = int{ ci.texture_x };
                data[i * 6 + 5] = int{ 0 };

                x += ci.advance * o6;
            }

            glNamedBufferData(chars_buf, text_c * 6 * sizeof(int32_t), data, GL_DYNAMIC_DRAW);
            ce;

            glClear(GL_COLOR_BUFFER_BIT);

            ce;
            glUseProgram(prog);
            glDispatchCompute(text_c, 1, 1);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
            ce;

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

            tmp = ptmp;

            next_redraw += frame_time;
            if(next_redraw < now && now - next_redraw >= frame_time) {
                next_redraw = now;
            }
        }
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
}
