// This file is part of EnvSearcher.
//
// EnvSearcher is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// EnvSearcher is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// EnvSearcher. If not, see <https://www.gnu.org/licenses/>.

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <assert.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "keyval.h"

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct {
    int arg_index;
    char delim;
    char* (*quote_fn)(char*, char*);
} options;

char* run_printf(char* key, char* value);
char* normal(char* key, char* value);

options parse_args(int argc, char** argv) {
    options ret = {
        .arg_index = 1,
        .delim = '\n',
        .quote_fn = &normal,
    };

    while (true) {
        const int opt = getopt(argc, argv, "zZqn");
        if (opt == -1) break;
        switch (opt) {
            case 'z': ret.delim = '\0';           break;
            case 'Z': ret.delim = '\n';           break;
            case 'q': ret.quote_fn = &run_printf; break;
            case 'n': ret.quote_fn = &normal;     break;
            case '?': exit(2);                    break;
        }
    }

    ret.arg_index = optind;
    return ret;
}

char* run_printf(char* key, char* value) {
    const int fd = memfd_create("output", 0);
    if (fd < 0) DIE("memfd_create");

    pid_t child = 0;
    char* argv[] = {"printf", "%s=%q", key, value, NULL};
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    posix_spawn_file_actions_addclose(&file_actions, STDIN_FILENO);
    posix_spawn_file_actions_addclose(&file_actions, STDERR_FILENO);
    posix_spawn_file_actions_adddup2(&file_actions, fd, STDOUT_FILENO);
    if (posix_spawnp(&child, "printf", &file_actions, NULL, argv, NULL)) DIE("posix_spawnp");
    posix_spawn_file_actions_destroy(&file_actions);

    int waitstatus = 0;
    const pid_t wait_result = waitpid(child, &waitstatus, 0);
    if (wait_result < 0) DIE("waitpid");
    if (wait_result != child) DIE("awaited wrong process");

    struct stat buf;
    if (fstat(fd, &buf) < 0) DIE("fstat");
    const size_t flen = buf.st_size;

    char* const ret = calloc(flen + 1, sizeof(*ret));
    if (!ret) DIE("calloc");

    if (lseek(fd, 0, SEEK_SET)) DIE("lseek");
    const ssize_t amount = read(fd, ret, flen);
    if (amount < 0) DIE("read");
    assert(((size_t)(amount)) == flen);

    if (close(fd)) DIE("close");

    return ret;
}

char* normal(char* key, char* value) {
    char* ret = NULL;
    asprintf(&ret, "%s=%s", key, value);
    return ret;
}

int main(int argc, char * argv[], char * envp[]) {
    const options options = parse_args(argc, &argv[0]);

    const int nargs = argc - options.arg_index;
    if (nargs > 1) exit(2);

    char* const needle = (nargs == 1) ? argv[options.arg_index] : "";
    const size_t needle_len = strlen(needle);

    for (char** cur = envp; *cur; ++cur) {
        keyval* const kv = keyval_new(*cur);
        if (!kv) continue;

        if (strstr(kv->key, needle)) {
            char* const message = options.quote_fn(kv->key, kv->value);
            printf("%s%c", message, options.delim);
            free(message);
        }

        free(kv);
    }

    return EXIT_SUCCESS;
}
