#include<cctype>
#include<stdio.h>
#include<stdlib.h>
#include<dirent.h>
#include<unistd.h>
#include<sys/wait.h>
#include<cstring>

#include"defs.h"
#include"alloc.h"

extern char **environ;

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

        execvpe((char*)args[0], (char **)args, environ);

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
            printf("Stderror is: |%.*s|", (int)(error.end - error.beg), error.beg);
            tmp = ptmp;

            return {};
        }
        else {
            close(stderror_fp[0]);
        }

        return output;
    }
}

bool open_url(str url) {
    memcpy(tmp, url.items, url.count);
    tmp[url.count] = '\0';
    printf("%s\n", tmp);

    // there's --new-tab flag, but it just breaks the CLI
    // and it opens an empty window. Whatever. Thank's for wasting my time
    char const *const tab_args[]{ "firefox", tmp, nullptr };
    var tab_res = run_sync(tab_args);
    if(tab_res.beg == nullptr) return false;

    char const *const focus_args[]{ "i3-msg", "[class=\"firefox\"]", "focus", nullptr };
    var focus_res = run_sync(focus_args);
    if(focus_res.beg == nullptr) return false;

    return true;
}
