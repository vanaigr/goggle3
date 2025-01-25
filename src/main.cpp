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

#define LCD 0

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
    int width; // 26.6
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
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, 1024, font_size);
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

        /*for(int i = 0; i < b.rows; i++) {
            for(int j = 0; j < b.width; j++) {
                printf("%c", b.buffer[i * b.width + j] ? '#' : ' ');
            }
            printf("\n");
        }
            printf("\n");*/


        ci = CharInfo{
            .advance = (int)g->advance.x,
            .width = (int)g->metrics.width,
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
        layout(rgba32f, binding = 0) uniform image2D outImg;

        //layout(rgba32f, binding = 1) uniform image2D glyphs;
        layout(std430, binding = 2) buffer Characters {
            int data[];
        };

        void main() {
            int groupId = int(gl_WorkGroupID.x);

            ivec4 character = ivec4(
                data[groupId * 4 + 0],
                data[groupId * 4 + 1],
                data[groupId * 4 + 2],
                data[groupId * 4 + 3]
            );

            int total = character.b * character.a;

            int off = int(gl_LocalInvocationIndex.x);
            for(; off < total; off += 64) {
                int x = off / character.a;
                int y = off % character.a;
                ivec2 coord = character.xy + ivec2(x, y);
                imageStore(outImg, coord, vec4(1, 1, 1, 1));
            }
        }
    )";

    #if 0
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
            ivec2 coord = ivec2(round(mix(vec2(off, size.y), vec2(off + w, 0), uv)));
        )"
    #if LCD
        R"(
            vec4 alpha = texelFetch(tex, coord, 0);
            result = vec4(
                1 * alpha.r,
                1 * alpha.g,
                1 * alpha.b,
                (alpha.r + alpha.g + alpha.b) * 0.33333
            );
        )"
    #else
        R"(
            float alpha = texelFetch(tex, coord, 0).r;
            result = vec4(1, 1, 1, alpha);
        )"
    #endif
        R"(
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
    #endif

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
    #if 0
    let loc = glGetUniformLocation(prog, "tex");
    glUniform1i(loc, 0);
    ce;

    let coord = glGetAttribLocation(prog, "coord");
    ce;

    let texture0 = glGetAttribLocation(prog, "texture");
    ce;
    #endif

    let loc = glGetUniformLocation(prog, "outImg");
    glUniform1i(loc, 0);

    let width_fac = 2.0 / width;
    let height_fac = 2.0 / height;

    #if 0
    GLuint va;
    glCreateVertexArrays(1, &va);
    glBindVertexArray(va);
    ce;

    GLuint vb;
    glCreateBuffers(1, &vb);
    glBindBuffer(GL_ARRAY_BUFFER, vb);

    glVertexAttribPointer(coord, 2, GL_FLOAT, false, 8, (void*)0);
    glEnableVertexAttribArray(coord);

    GLuint vb2;
    glCreateBuffers(1, &vb2);
    glBindBuffer(GL_ARRAY_BUFFER, vb2);

    if(texture0 > 0) {
        glVertexAttribIPointer(texture0, 2, GL_INT, 8, (void*)0);
        glEnableVertexAttribArray(texture0);
    }

    glBindVertexArray(0);
    ce;
    #endif

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

    glEnable(GL_BLEND);
#if LCD
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
#else
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

    XMapWindow(display, window);

    bool keming = true;

    let text = tmp;
    let text_end = tmp + 256;
    var text_c = 5;
    memcpy(text, "aaaaa", 5);
    tmp += 256;

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

        let ptmp = tmp;

        let data = talloc<int32_t>(text_c * 4);

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

            let gw = ci.width >> 6;
            let gh = font_size;

            data[i * 4 + 0] = int{ (int)std::floor(x) };
            data[i * 4 + 1] = int{ (int)std::floor(y) };
            data[i * 4 + 2] = int{ gw };
            data[i * 4 + 3] = int{ gh };

            x += ci.advance * o6;
        }

        glNamedBufferData(chars_buf, text_c * 4 * sizeof(int32_t), data, GL_DYNAMIC_DRAW);
        ce;

        #if 0

        let points = talloc<float>(12 * text_c);
        let textures = talloc<int>(12 * text_c);

        let o6 = 1.0f / 64;

        float x = 0;
        float y = 0;
        for(int i = 0; i < text_c; i++) {
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

            let gw = ci.width * o6;
            let gh = font_size;

            points[i*12 + 0] = left_off * width_fac;
            points[i*12 + 1] = top_off * height_fac;
            points[i*12 + 2] = left_off * width_fac;
            points[i*12 + 3] = (top_off + gh) * height_fac;
            points[i*12 + 4] = (left_off + gw) * width_fac;
            points[i*12 + 5] = top_off * height_fac;

            points[i*12 + 6] = left_off * width_fac;
            points[i*12 + 7] = (top_off + gh) * height_fac;
            points[i*12 + 8] = (left_off + gw) * width_fac;
            points[i*12 + 9] = top_off * height_fac;
            points[i*12 + 10] = (left_off + gw) * width_fac;
            points[i*12 + 11] = (top_off + gh) * height_fac;

            x += ci.advance * o6;
        }
        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferData(
            GL_ARRAY_BUFFER,
            text_c * 12 * sizeof(float),
            points,
            GL_DYNAMIC_DRAW
        );
        ce;

        glBindBuffer(GL_ARRAY_BUFFER, vb2);
        for(int i = 0; i < text_c; i++) {
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
        glBufferData(
            GL_ARRAY_BUFFER,
            text_c * 12 * sizeof(int),
            textures,
            GL_DYNAMIC_DRAW
        );
        ce;

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(va);
        glDrawArrays(GL_TRIANGLES, 0, 6 * text_c);
        glBindVertexArray(0);
        ce;

        #endif

        ce;
        glUseProgram(prog);
glDispatchCompute(100, 1, 1);
glMemoryBarrier(GL_ALL_BARRIER_BITS);
        ce;

        glBlitNamedFramebuffer(
            fb, 0,
            0, 0, width, height,
            0, 0, width, height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );
        ce;

        glXSwapBuffers(display, window);
        tmp = ptmp;
        ce;
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
