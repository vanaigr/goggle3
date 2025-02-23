#include<stdio.h>
#include<GL/glew.h>
#include<limits>
#include<assert.h>

#include"alloc.h"
#include"defs.h"

int reserveBytes(GLBuffer &buf, int byte_count) {
    if(buf.capacity - buf.count < byte_count) {
        let ptmp = tmp;

        let max = std::numeric_limits<int>::max() >> 1;

        var new_cap = buf.capacity <= 0 ? 1 : buf.capacity;
        while(new_cap - buf.count < byte_count) {
            assert(new_cap <= max);
            new_cap <<= 1;
        }

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
