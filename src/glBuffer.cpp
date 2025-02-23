#include<stdio.h>
#include<GL/glew.h>
#include<limits>
#include<assert.h>

#include"alloc.h"
#include"defs.h"

static constexpr let max = std::numeric_limits<int>::max() >> 1;

int reserveBytes(GLBuffer &buf, int byte_count) {
    var new_cap = buf.capacity <= 0 ? 1 : buf.capacity;
    while(new_cap - buf.count < byte_count) {
        assert(new_cap <= max);
        new_cap <<= 1;
    }
    if(new_cap != buf.capacity) {
        let ptmp = tmp;

        let space = alloc(buf.count);
        glGetNamedBufferSubData(buf.buffer, 0, buf.count, space);

        glNamedBufferData(buf.buffer, new_cap, nullptr, GL_DYNAMIC_DRAW);
        glNamedBufferSubData(buf.buffer, 0, buf.count, space);

        buf.capacity = new_cap;

        tmp = ptmp;
    }

    let off = buf.count;
    buf.count += byte_count;

    return off;
}
