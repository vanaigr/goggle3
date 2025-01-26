#include<ft2build.h>
#include FT_FREETYPE_H
#include<GL/glew.h>
#include<GL/glx.h>
#include<cmath>

#include"defs.h"
#include"alloc.h"

#define LCD 1

static FT_Library F;
static FT_Face face;
static GLuint texture;
static GLuint chars_buf;
static GLuint prog;

static int const font_size = 24;

typedef struct {
    int texture_w, texure_h;
    int texture_x;
    int glyph_index;
    int16_t left_off;
    int16_t top_off;
    int16_t advance;
    bool initialized;
} CharInfo;

static CharInfo chars16[200000];

static var off = chars16[0].texture_w;


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

int text_init() {
    FT_Init_FreeType(&F);

    FT_New_Face(F, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &face);
    if(FT_Set_Pixel_Sizes(face, 0, font_size)) {
        printf("font where?\n");
        return 1;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
#if LCD
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1024, font_size);
#else
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 1024, font_size);
#endif

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, texture);
    // why????? why can't I just unactivate the texture?
    glActiveTexture(GL_TEXTURE0);

    glCreateBuffers(1, &chars_buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chars_buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chars_buf);
    ce;

    let compute = glCreateShader(GL_COMPUTE_SHADER);
    let compute_src = R"(
        #version 440

        layout(local_size_x = 64, local_size_y = 1) in;
        layout(rgba32f, binding = 1) coherent uniform image2D outImg;

        layout(rgba32f, binding = 2) readonly uniform image2D glyphs;
        layout(std430, binding = 1) buffer Characters {
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
                ivec2 glyphs_coord = texture_off + ivec2(x, character.a-1 - y);
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

    prog = glCreateProgram();
    glAttachShader(prog, compute);
    glLinkProgram(prog);
    programReport(prog);
    ce;

    glUseProgram(prog);

    let loc = glGetUniformLocation(prog, "outImg");
    glUniform1i(loc, 1);

    #if LCD
    // no docs, no nothing. Guess yourself that images DON'T SUPPORT RGB8, ONLY RGBA8
    glBindImageTexture(2, texture, 0, false, 0, GL_READ_ONLY, GL_RGBA8);
    #else
    glBindImageTexture(2, texture, 0, false, 0, GL_READ_ONLY, GL_R8);
    #endif
    ce;

    return 0;
}

void text_bind_texture(GLint tex) {
    glBindImageTexture(1, tex, 0, false, 0, GL_READ_WRITE, GL_RGBA8);
}

static CharInfo get_glyph(int code) {
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

        ci = CharInfo{
            .texture_w = 0,
            .texure_h = 0,
            .texture_x = 0,
            .glyph_index = glyph_index,
            .left_off = 0,
            .top_off = 0,
            .advance = 0,
            .initialized = true,
        };
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
        .texture_w = (int)gl_texture_w,
        .texure_h = (int)b.rows,
        .texture_x = off,
        .glyph_index = glyph_index,
        .left_off = (int16_t)(g->bitmap_left),
        .top_off = (int16_t)(g->bitmap_top),
        .advance = (int16_t)(g->advance.x >> 6),
        .initialized = true,
    };

    off += gl_texture_w;

    return ci;
}

void text_draw(char *text, int text_c, int font_size, int x, int y, int max_width) {
    let ptmp = tmp;

    let data = talloc<int32_t>(text_c * 6);

    var charCount = 0;

    for(var i = 0; i < text_c; i++) {
        let c = text[i];
        let ci = get_glyph(c);
        if(c == '\n') {
            x = 0;
            y -= font_size * 1.25;
            continue;
        }

        if(i != 0) {
            FT_Vector kerning;
            let error = FT_Get_Kerning(
                face,
                chars16[text[i-1]].glyph_index,
                ci.glyph_index,
                FT_KERNING_DEFAULT,
                &kerning
            );
            x += kerning.x >> 6;
        }

        data[charCount * 6 + 0] = int{ x + ci.left_off };
        data[charCount * 6 + 1] = int{ y + ci.top_off - ci.texure_h };
        data[charCount * 6 + 2] = int{ ci.texture_w };
        data[charCount * 6 + 3] = int{ ci.texure_h };
        data[charCount * 6 + 4] = int{ ci.texture_x };
        data[charCount * 6 + 5] = int{ 0 };
        charCount++;

        x += ci.advance;
    }

    glNamedBufferData(chars_buf, charCount * 6 * sizeof(int32_t), data, GL_DYNAMIC_DRAW);
    ce;

    glClear(GL_COLOR_BUFFER_BIT);

    ce;
    glUseProgram(prog);
    glDispatchCompute(text_c, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    ce;

    tmp = ptmp;
}
