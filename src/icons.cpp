#include<png.h>
#include<stdint.h>
#include<algorithm>
#include<GL/glew.h>

#include"defs.h"
#include"alloc.h"

static int ilookup(uint8_t code_) {
    char code = code_;
    if(code >= 'A' && code <= 'Z') return code - 'A';
    if(code >= 'a' && code <= 'z') return code - 'a' + ('Z' - 'A' + 1);
    if(code >= '0' && code <= '9') return code - '0' + ('z' - 'a' + 1) + ('Z' - 'A' + 1);
    if(code == '+') return 62;
    if(code == '/') return 63;
    return -1;
}

static bool dr(uint8_t *&out, uint8_t *b, uint8_t *e) {
    #define il(name, v) let name = ilookup((v)); if(name < 0) return false

    while(e > b && e[-1] == '=') e--;
    let diff = e - b;
    if(diff == 0) return true;
    if(diff == 1) return false;

    il(i1, b[0]);
    il(i2, b[1]);
    *out++ = (i1 << 2) | (i2 >> 4);

    if(diff == 2) return true;

    il(i3, b[2]);
    *out++ = (i2 << 4) | (i3 >> 2);
    if(diff == 3) return true;

    il(i4, b[3]);
    *out++ = (i3 << 6) | i4;

    return true;
}

static char *decodeBase640(uint8_t *b, uint8_t *e) {
    var res = (uint8_t*)tmp;
    while(b < e) {
        if(!dr(res, b, e)) return nullptr;
        b += std::min<int>(e - b, 4);
    }
    return (char*)res;
}

static str decodeBase64(uint8_t *b, uint8_t *e) {
    let ptmp = tmp;
    let res = decodeBase640(b, e);
    if(res != nullptr) {
        tmp = res;
        return mkstr(ptmp, res);
    }
    return { nullptr, 0 };
}

static bool inited;
static GLuint srcFB, dstFB;
static GLuint dstT;

static constexpr let icon_size = 24;

static void init() {
    if(inited) return;

    glGenFramebuffers(1, &srcFB);
    glGenFramebuffers(1, &dstFB);

    glGenTextures(1, &dstT);
    glBindTexture(GL_TEXTURE_2D, dstT);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, icon_size, icon_size);
    ce;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFB);
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dstT, 0);
    ce;

    if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("dst framebuffer?\n");
        exit(1);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFB);

    inited = true;
}

static Texture decodePng(uint8_t *b, uint8_t *e, GLBuffer &buf) {
    let res = decodeBase64(b, e);
    if(res.items == nullptr) return { -1, 0, 0 };

    let ptmp = tmp;
    var bitmap = decode_png_rgba((uint8_t*)res.items, res.count);
    if(bitmap.data == nullptr) return { -1, 0, 0 };
    var dataSize = bitmap.width * bitmap.height * 4;

    if(bitmap.width != icon_size || bitmap.height != icon_size) {
        init();

        GLuint srcT;
        glGenTextures(1, &srcT);
        glBindTexture(GL_TEXTURE_2D, srcT);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, bitmap.width, bitmap.height);
        glTexSubImage2D(
            GL_TEXTURE_2D, 0,
            0, 0, bitmap.width, bitmap.height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            bitmap.data
        );
        ce;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFB);
        glFramebufferTexture(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, srcT, 0);
        ce;

        if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            printf("src framebuffer?\n");
            exit(1);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFB);
        glBlitFramebuffer(
            0, 0, bitmap.width, bitmap.height,
            0, 0, icon_size, icon_size,
            GL_COLOR_BUFFER_BIT,
            GL_LINEAR
        );
        ce;

        glDeleteTextures(1, &srcT);
        ce;

        bitmap = { (uint8_t*)ptmp, icon_size, icon_size };
        dataSize = bitmap.width * bitmap.height * 4;
        tmp = (char*)bitmap.data + dataSize;
        glGetTextureImage(dstT, 0, GL_RGBA, GL_UNSIGNED_BYTE, dataSize, bitmap.data);
        ce;
    }

    let off = reserveBytes(buf, dataSize);
    glNamedBufferSubData(buf.buffer, off, dataSize, bitmap.data);

    return { off, bitmap.width, bitmap.height };
}

Texture decodeIcon(str string, GLBuffer &buf) {
    let prefix = STR("data:image/png;base64,");
    if(string.count >= prefix.count && streq({ string.items, prefix.count }, prefix)) {
        let ptmp = tmp;
        let res = decodePng(
            (uint8_t*)(string.items + prefix.count),
            (uint8_t*)(string.items + string.count),
            buf
        );
        tmp = ptmp;
        return res;
    }
    return { -1, 0, 0 };
}
