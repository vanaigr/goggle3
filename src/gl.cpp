#include<GL/glew.h>
#include<GL/glx.h>
#include<stdio.h>

void check_error(char const *file, int line) {
    int err = glGetError();
    if(err) {
        printf("[%s %d] What is %d\n", file, line, err);
        fflush(stdout);
        throw 1;
    }
}
