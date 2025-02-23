#include<png.h>
#include<stdint.h>
#include<algorithm>
#include<GL/glew.h>

#include"defs.h"
#include"alloc.h"

uint8_t *decode_png_rgba(const uint8_t *png_data, size_t png_size, int *out_width, int *out_height);

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

static Texture decodePng(uint8_t *b, uint8_t *e, GLBuffer &buf) {
    let res = decodeBase64(b, e);
    if(res.items == nullptr) return { -1, 0, 0 };

    int width = 0, height = 0;
    let data = decode_png_rgba((uint8_t*)res.items, res.count, &width, &height);
    if(data == nullptr) return { -1, 0, 0 };
    let dataSize = width * height * 4;

    let off = reserveBytes(buf, dataSize);
    glNamedBufferSubData(buf.buffer, off, dataSize, data);

    return { off, width, height };
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

#include<cassert>
int main3() {
    let in = STR("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABwAAAAcCAYAAAByDd+UAAAA7ElEQVR4AWJwLwC0WwcaCENhFMefoV6hABgEBhDAxN7jAggRPcSeoAcYQFgIInuSIcAlwLqHM45klvtt0PCXO7Pfvu3Gskn7c3AG/WldhNIpwRaNDQJadb9EH9agYiWQ13G55doRPYwFLkKNIpiQ68QcZIqUXHuszUBMo1PJLm2JJ/qoo0FcSKZCBdGc62asd5h8wkRTKxCIkwsvZPMU+h7tQP4VNB5zehNR4LWq6tD+y+N0xHyH8/wzigFbXEA2TColiMed3ODTBMQUPXlzEJP0ZQnWl9t9MyQr8KdiwTy0C2WD4rnzN80MmvcG9xb1UQNO3ZEAAAAASUVORK5CYII=");

    GLBuffer buf;
    decodeIcon(in, buf);
}
