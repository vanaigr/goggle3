#include<cctype>
#include<stdio.h>
#include<stdlib.h>
#include<dirent.h>
#include<unistd.h>
#include<sys/wait.h>
#include<cassert>

#include"defs.h"
#include"alloc.h"

struct span {
    char *beg;
    char *end;
};

static span drain(int pipe) {
    char *begin = tmp;

    ssize_t n;
    do {
        n = read(pipe, tmp, tmp_end - tmp);
        tmp += n;
    } while(n > 0);

    return { begin, tmp };
}

static span run_sync(char const *const *args) {
    int stdout_fp[2];
    if (pipe(stdout_fp) == -1) {
        perror("pipe died");
        return {};
    }
    int stderror_fp[2];
    if (pipe(stderror_fp) == -1) {
        perror("pipe died");
        return {};
    }

    let pid = fork();
    if(pid < 0) {
        perror("died\n");
        return {};
    }
    else if(pid == 0) {
        close(stdout_fp[0]);
        dup2(stdout_fp[1], STDOUT_FILENO);
        close(stdout_fp[1]);

        close(stderror_fp[0]);
        dup2(stderror_fp[1], STDERR_FILENO);
        close(stderror_fp[1]);

        execvp((char*)args[0], (char **)args);

        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else {
        close(stdout_fp[1]);
        close(stderror_fp[1]);

        let output = drain(stdout_fp[0]);
        close(stdout_fp[0]);

        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return {};
        }

        if(status != EXIT_SUCCESS) {
            let ptmp = tmp;
            let error = drain(stderror_fp[0]);
            close(stderror_fp[0]);
            printf("Error for %s: status is %d\n", args[0], status);
            printf("Stderror is: |%.*s|", error.end - error.beg, error.beg);
            tmp = ptmp;

            return {};
        }
        else {
            close(stderror_fp[0]);
        }

        return output;
    }
}

enum JsonType {
    object,
    array,
    string,
};

struct Json {
    JsonType type;
    str name;
    union {
        Json *end;
        str string;
    };
};

static bool isspace(char c) {
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

static char const *readStr(char const *cur, char const *end) {
    var backspace = false;
    while(cur < end) {
        let c = *cur;
        if(!backspace && c == '"') break;
        backspace = !backspace && c == '\\';
        cur++;
    }
    return cur;
}

#define ss ([&]() { while(cur < end && isspace(*cur)) cur++; })()

static void report(int line, char const *cur, char const *end) {
    if(cur == end) {
        printf("Error on line %d (eol)\n", line);
    }
    else {
        printf("Error on line %d (char is %c)\n", line, *cur);
    }
}

static char const *decodeJson(Json *&end_obj, str name, char const *cur, char const *end) {
    #define err do { report(__LINE__, cur, end); return cur; } while(0)

    ss;
    if(cur == end) err;
    else if(*cur == '{') {
        cur++;
        var new_end = end_obj + 1;

        var i = 0;
        while(true) {
            ss;
            if(cur >= end) err;
            if(*cur == '}') {
                cur++;
                break;
            }
            if(i != 0) {
                if(*cur != ',') err;
                cur++;
                ss;
                if(cur >= end) err;
            }
            if(*cur != '"') err;
            cur++;

            let beg = cur;
            cur = readStr(cur, end);
            if(cur >= end || *cur != '"') err;
            cur++;
            ss;
            if(cur >= end || *cur != ':') err;
            cur++;

            let name = mkstr(beg, cur);
            cur = decodeJson(new_end, name, cur, end);
            i++;
        }

        *end_obj = {
            .type = object,
            .name = name,
            .end = new_end,
        };
        end_obj = new_end;
    }
    else if(*cur == '[') {
        cur++;
        var new_end = end_obj + 1;

        var i = 0;
        while(true) {
            ss;
            if(cur >= end) err;
            if(*cur == ']') {
                cur++;
                break;
            }
            if(i != 0) {
                if(*cur != ',') err;
                cur++;
                ss;
                if(cur >= end) err;
            }

            cur = decodeJson(new_end, {}, cur, end);
            i++;
        }

        *end_obj = {
            .type = array,
            .name = name,
            .end = new_end,
        };
        end_obj = new_end;
    }
    else if(*cur == '"') {
        cur++;
        let beg = cur;
        cur = readStr(cur, end);
        let str_end = cur;
        if(cur >= end || *cur != '"') err;
        cur++;

        *end_obj++ = {
            .type = string,
            .name = name,
            .string = mkstr(beg, str_end)
        };
    }
    else if(*cur == '-' || (*cur >= '0' && *cur <= '9')) {
        cur++;
        for(; cur < end; cur++) {
            let c = *cur;

            if(c >= '0' && c <= '9') continue;
            #define a(ch) if(c == ch) continue;
            a('-');
            a('+');
            a('.');
            a('e');
            a('E');
            #undef a

            break;
        }
    }
    else if(*cur == 't') {
        if(cur + 3 >= end) err;
        cur++;
        if(*cur++ != 'r') err;
        if(*cur++ != 'u') err;
        if(*cur++ != 'e') err;
    }
    else if(*cur == 'f') {
        if(cur + 4 >= end) err;
        cur++;
        if(*cur++ != 'a') err;
        if(*cur++ != 'l') err;
        if(*cur++ != 's') err;
        if(*cur++ != 'e') err;
    }
    else if(*cur == 'n') {
        if(cur + 3 >= end) err;
        cur++;
        if(*cur++ != 'u') err;
        if(*cur++ != 'l') err;
        if(*cur++ != 'l') err;
    }
    else {
        err;
    }

    return cur;
#undef err
}

static Json *decode(span input) {
    let array = (Json*)align(tmp, 6);
    var arr_end = array;

    var cur = (char const *)input.beg;
    let end = input.end;
    cur = decodeJson(arr_end, {}, cur, end);
    ss;
    if(cur != end) {
        printf("Should not happen\n");
    }
    tmp = (char*)arr_end;

    return array;
}
#undef ss

static bool findWindow(Json *itp) {
    let &it = *itp;
    assert(it.type == array);

    var cur = itp + 1;
    let end = it.end;
    while(cur < end) {
        let &c = *cur;
        if(c.type == object) {

        }
    }
}

static void checkWorkspaces(Json *itp) {
    let &it = *itp;
    if(it.type == array) {
        var cur = itp + 1;
        let end = it.end;
        while(cur < end) {
            let &c = *cur;
            if(c.type == object || c.type == array) {
                checkWorkspaces(cur);
                cur = c.end;
            }
            else {
                assert(cur->type == string);
                cur++;
            }
        }
    }
    else if(it.type == object) {
        var isWorkspace = false;
        var nodes = (Json*)nullptr;
        var floatingNodes = (Json*)nullptr;

        var cur = itp + 1;
        let end = it.end;
        while(cur < end) {
            let &c = *cur;
            if(c.type == string) {
                if(streq(c.name, STR("type"))) {
                    if(streq(c.string, STR("workspace"))) {
                        isWorkspace = true;
                    }
                    else break;
                }
                else if(streq(c.name, STR("nodes"))) {
                    nodes = cur;
                }
                else if(streq(c.name, STR("floating_nodes"))) {
                    nodes = cur;
                }
                cur = cur + 1;
            }
            else {
                assert(c.type == object || c.type == array);
                cur = cur->end;
            }
        }

        if(!isWorkspace || !nodes || nodes->type != array) return;
        findWindow(nodes);
    }
}

void search_process() {
    char const *const get_tree_args[]{ "i3-msg", "[class=\"firefox\"]", "focus", nullptr };
    var tree_res = run_sync(get_tree_args);
}
