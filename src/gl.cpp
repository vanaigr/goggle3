#include<GL/glew.h>
#include<GL/glx.h>
#include<stdio.h>

#include"alloc.h"

void check_error(char const *file, int line) {
    int err = glGetError();
    if(err) {
        printf("[%s %d] What is %d\n", file, line, err);
        fflush(stdout);
        throw 1;
    }
}

void shaderReport_(char const *file, int line, GLint shader, char const *name) {
    GLint res;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
    if(!res) {
        GLint len;
        glGetShaderInfoLog(shader, tmp_end - tmp, &len, tmp);
        printf("[%s %d] %s shader said %.*s", file, line, name, len, tmp);
        fflush(stdout);
        throw 1;
    }
}

void programReport_(char const *file, int line, GLint prog, char const *name) {
    GLint res;
    glGetProgramiv(prog, GL_LINK_STATUS, &res);
    if(!res) {
        GLint len;
        glGetProgramInfoLog(prog, tmp_end - tmp, &len, tmp);
        printf("[%s %d] %s program said %.*s", file, line, name, len, tmp);
        fflush(stdout);
        throw 1;
    }
}

