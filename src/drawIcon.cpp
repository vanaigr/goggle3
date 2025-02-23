#include<cstring>
#include<GL/glew.h>
#include<GL/glx.h>
#include<cmath>

#include"defs.h"
#include"alloc.h"

static GLBuffer *buf;
static GLuint imageCoords;
static GLuint prog;
static GLuint descs;

int image_init(GLBuffer *buf) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, buf->buffer);
    ce;

    glCreateBuffers(1, &descs);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, descs);

    let compute = glCreateShader(GL_COMPUTE_SHADER);
    let compute_src = R"(
        #version 440

        layout(local_size_x = 64, local_size_y = 1) in;
        layout(rgba32f, binding = 1) coherent uniform image2D outImg;

        layout(std430, binding = 4) readonly buffer Textures {
            // ints. rgba in some order
            uint texture_data[];
        };
        layout(std430, binding = 5) readonly buffer TextureDescs {
            // x, y, width, height, off, pad
            int desc_data[];
        };

        void main() {
            int groupId = int(gl_WorkGroupID.x);

            ivec4 texture_desc = ivec4(
                desc_data[groupId * 6 + 0],
                desc_data[groupId * 6 + 1],
                desc_data[groupId * 6 + 2],
                desc_data[groupId * 6 + 3]
            );
            int texture_off = desc_data[groupId * 6 + 4];

            int total = texture_desc.b * texture_desc.a;

            int off = int(gl_LocalInvocationIndex.x);
            for(; off < total; off += 64) {
                int x = off % texture_desc.b;
                int y = off / texture_desc.b;
                ivec2 coord = texture_desc.xy + ivec2(x, y);

                uint rgb = texture_data[texture_off + off];
                vec4 col = vec4(
                    rgb & 255u,
                    (rgb >> 8u) & 255u,
                    (rgb >> 16u) & 255u,
                    (rgb >> 24u) & 255u
                );
                col *= 1.0 / 255.0;

                vec4 prev = imageLoad(outImg, coord);
                vec4 color = vec4(mix(prev.rgb, col.rgb, col.a), 1);
                imageStore(outImg, coord, color);
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

    return 0;
}

void image_draw(TextureDescs dl) {
    let ptmp = tmp;

    let data = talloc<uint32_t>(dl.count * 6);
    for(var i = 0; i < dl.count; i++) {
        let base = i * 6;
        let it = dl.items[i];

        data[base + 0] = it.x;
        data[base + 1] = it.y;
        data[base + 2] = it.w;
        data[base + 3] = it.h;
        data[base + 4] = it.off / 4;
    }

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glNamedBufferData(
        descs, dl.count * 6 * sizeof(int32_t),
        data, GL_DYNAMIC_DRAW
    );
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    ce;

    tmp = ptmp;

    ce;
    glUseProgram(prog);
    glDispatchCompute(dl.count, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    ce;
}
