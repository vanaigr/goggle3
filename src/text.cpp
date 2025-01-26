#include<ft2build.h>
#include FT_FREETYPE_H
#include<GL/glew.h>
#include<GL/glx.h>
#include<cmath>
#include<array>
#include<unordered_map>

#include"defs.h"
#include"alloc.h"
#include"text.h"

#define LCD 1

static FT_Library F;
static GLuint glyph_buf;
static GLuint chars_buf;
static GLuint prog;

static struct {
    GLuint buf;
    int size;
    int cap;
} glyphs;

struct CharInfo {
    int glyphs_off;
    int width, height;
    int glyph_index;
    int16_t left_off;
    int16_t top_off;
    int16_t advance;
    bool initialized;
};

constexpr int chars_bucket_shift = 8;
using CharsBucket = std::array<CharInfo, 1 << chars_bucket_shift>;
using Chars = std::array<CharsBucket*, 600>;

struct FontInfo {
    FT_Face face;
    Chars chars;
};

struct FontDefHash {
    std::size_t operator()(const FontDef k) const {
        return std::hash<uint64_t>()(uint32_t(k.font_size) + (uint64_t(k.bold) << 32));
    }
};

// by font size
static std::unordered_map<FontDef, FontInfo*, FontDefHash> fonts{};

FontInfo *get_font_info(FontDef fd) {
    var &f = fonts[fd];
    if(f == nullptr) {
        f = new FontInfo{};
        if(fd.bold) {
            FT_New_Face(F, "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 0, &f->face);
        }
        else {
            FT_New_Face(F, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &f->face);
        }
        if(FT_Set_Pixel_Sizes(f->face, 0, fd.font_size)) {
            printf("font where?\n");
            throw 1;
        }
    }

    return f;
}

int text_init() {
    FT_Init_FreeType(&F);

    glCreateBuffers(1, &chars_buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, chars_buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, chars_buf);
    ce;

    let compute = glCreateShader(GL_COMPUTE_SHADER);
    let compute_src = R"(
        #version 440

        layout(local_size_x = 64, local_size_y = 1) in;
        layout(rgba32f, binding = 1) coherent uniform image2D outImg;

        layout(std430, binding = 1) buffer Characters {
            // who needs structs if they must be aligned?
            int data[];
        };
        layout(std430, binding = 2) readonly buffer Glyphs {
            uint glyph_data[];
        };

        void main() {
            int groupId = int(gl_WorkGroupID.x);

            ivec4 character = ivec4(
                data[groupId * 6 + 0],
                data[groupId * 6 + 1],
                data[groupId * 6 + 2],
                data[groupId * 6 + 3]
            );

            int glyphs_off = data[groupId * 6 + 4];

            int total = character.b * character.a;

            int off = int(gl_LocalInvocationIndex.x);
            for(; off < total; off += 64) {
                int x = off % character.b;
                int y = off / character.b;
                ivec2 coord = character.xy + ivec2(x, y);
    )"

    #if LCD
    R"(
                int this_off = glyphs_off + off;
                uint rgb = glyph_data[this_off];
                vec3 alpha = vec3(rgb & 255u, (rgb >> 8u) & 255u, (rgb >> 16u) & 255u);
                alpha *= 1.0 / 255.0;

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

    return 0;
}

void text_bind_texture(GLint tex) {
    glBindImageTexture(1, tex, 0, false, 0, GL_READ_WRITE, GL_RGBA8);
}

// returns uint32_t index
static int glyphs_add(int byte_count, char const *data) {
    int uints_count = ((byte_count >> 2) + (byte_count & ((1 << 2) - 1)));

    // force init even if byte_count is 0.
    // Not that this would ever happen, but this is correct behavior.
    if(glyphs.cap == 0) {
        GLuint buf;
        glGenBuffers(1, &buf);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buf);

        let cap = std::max(1 << 12, uints_count);
        glNamedBufferData(buf, cap * 4, nullptr, GL_DYNAMIC_DRAW);

        glyphs.buf = buf;
        glyphs.size = uints_count;
        glyphs.cap = cap;
    }
    else if(glyphs.cap - glyphs.size < uints_count) {
        let ptmp = tmp;

        var new_cap = glyphs.cap << 1;
        while(new_cap - glyphs.size < uints_count) new_cap <<= 1;

        let space = talloc<uint32_t>(glyphs.size);
        glGetNamedBufferSubData(glyphs.buf, 0, glyphs.size * 4, space);

        glNamedBufferData(glyphs.buf, new_cap * 4, nullptr, GL_DYNAMIC_DRAW);
        glNamedBufferSubData(glyphs.buf, 0, glyphs.size * 4, space);

        glyphs.cap = new_cap;

        tmp = ptmp;
    }

    let off = glyphs.size;
    glNamedBufferSubData(glyphs.buf, off * 4, byte_count, data);
    glyphs.size += uints_count;

    return off;
}

static CharInfo get_glyph(FontInfo *font_info, int code) {
    var &fi = *font_info;

    let bucket_i = code >> chars_bucket_shift;
    let in_bucket_i = code & ((1 << chars_bucket_shift) - 1);
    var bucket = fi.chars[bucket_i];
    if(!bucket) {
        bucket = new CharsBucket{};
        fi.chars[bucket_i] = bucket;
    }

    var &ci = (*bucket)[in_bucket_i];
    if(ci.initialized) {
        return ci;
    }

    int glyph_index = FT_Get_Char_Index(fi.face, code);
    if(FT_Load_Glyph(
        fi.face,
        glyph_index,
        FT_LOAD_RENDER
    #if LCD
            | FT_LOAD_TARGET_LCD
    #endif
    )) {
        printf("died on %d \n", code);

        char empty;
        glyphs_add(0, &empty);

        ci = CharInfo{
            .glyphs_off = 0,
            .width = 0,
            .height = 0,
            .glyph_index = glyph_index,
            .left_off = 0,
            .top_off = 0,
            .advance = 0,
            .initialized = true,
        };
        return ci;
    }

    let g = fi.face->glyph;
    let b = g->bitmap;

    int texture_w;
#if LCD
    let ptmp = tmp;
    texture_w = b.width / 3;
    let texture_c = texture_w * b.rows;
    let ptr_base = talloc<uint32_t>(texture_c);

    var bmp_row_off = 0;
    for(var y = b.rows - 1; y != -1; y--) {
        var bmp_off = bmp_row_off;
        for(var x = 0; x < texture_w; x++) {
            let re = b.buffer[bmp_off];
            let gr = b.buffer[bmp_off + 1];
            let bl = b.buffer[bmp_off + 2];
            ptr_base[y * texture_w + x] = (re) | (gr << 8) | (bl << 16);
            bmp_off += 3;
        }
        bmp_row_off += b.pitch;
    }

    let off = glyphs_add(4 * texture_c, (char*)ptr_base);

    tmp = ptmp;
#else
// todo: reverse
    texture_w = b.width;
    let texture_c = texture_w * b.rows;
    let off = glyphs_add(texture_c, (char*)b.buffer);
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
        .glyphs_off = off,
        .width = (int)texture_w,
        .height = (int)b.rows,
        .glyph_index = glyph_index,
        .left_off = (int16_t)(g->bitmap_left),
        .top_off = (int16_t)(g->bitmap_top),
        .advance = (int16_t)(g->advance.x >> 6),
        .initialized = true,
    };

    return ci;
}

void text_draw(
    char const *text,
    int text_c,
    FontDef fd,
    int begin_x,
    int begin_y,
    int max_width
) {
    let ptmp = tmp;

    let fi = get_font_info(fd);

    let data = talloc<int32_t>(text_c * 6);

    var charCount = 0;
    var prev_glyph_index = 0;

    var x = begin_x;
    var y = begin_y;
    for(var i = 0; i < text_c; i++) {
        let c = text[i];
        let ci = get_glyph(fi, c);
        if(c == '\n') {
            x = begin_x;
            y -= fd.font_size * 1.25;
            prev_glyph_index = 0;
            continue;
        }
        else if(c == ' ') {
            x += ci.advance;
            prev_glyph_index = ci.glyph_index;
            continue;
        }

        if(prev_glyph_index != 0) {
            FT_Vector kerning;
            let error = FT_Get_Kerning(
                fi->face,
                prev_glyph_index,
                ci.glyph_index,
                FT_KERNING_DEFAULT,
                &kerning
            );
            x += kerning.x >> 6;
        }

        data[charCount * 6 + 0] = int{ x + ci.left_off };
        data[charCount * 6 + 1] = int{ y + ci.top_off - ci.height };
        data[charCount * 6 + 2] = int{ ci.width };
        data[charCount * 6 + 3] = int{ ci.height };
        data[charCount * 6 + 4] = int{ ci.glyphs_off };
        charCount++;

        x += ci.advance;
        prev_glyph_index = ci.glyph_index;
    }

    glNamedBufferData(chars_buf, charCount * 6 * sizeof(int32_t), data, GL_DYNAMIC_DRAW);
    ce;

    ce;
    glUseProgram(prog);
    glDispatchCompute(text_c, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    ce;

    tmp = ptmp;
}
