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

#include <assert.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "quote.h"

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

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
    static const char kEquals[] = "=";
    const size_t eq_len = strlen(kEquals);

    const size_t key_len = strlen(key);
    const size_t val_len = strlen(value);

    const size_t ret_len = key_len + eq_len + val_len + 1;
    char* ret = calloc(ret_len, sizeof(*ret));

    char* cur = ret;
    cur = mempcpy(cur, key, key_len);
    cur = mempcpy(cur, &kEquals[0], eq_len);
    cur = mempcpy(cur, value, val_len);

    return ret;
}

char* hex_encode(char* key, char* value) {
    static const char kTable[] = "0123456789abcdef";

    static const char kAssignment[] = "=$'";
    static const char kHexPrefix[] = "\\x";
    static const char kCloseQuote[] = "'";

    const size_t keylen = strlen(key);
    const size_t vallen = strlen(value);
    const size_t retsize = keylen + 3 + vallen * 4 + 2;
    char* const ret = calloc(retsize, sizeof(*ret));

    char* cur = ret;
    cur = mempcpy(cur, key, keylen);
    cur = mempcpy(cur, kAssignment, strlen(kAssignment));

    for (unsigned i = 0; i < vallen; ++i) {
        const int val = value[i];

        cur = mempcpy(cur, kHexPrefix, strlen(kHexPrefix));
        cur = mempcpy(cur, &kTable[(val >> 4) & 0x0f], 1);
        cur = mempcpy(cur, &kTable[(val >> 0) & 0x0f], 1);
    }

    *cur = '\'';
    cur += 1;
    assert(cur == &ret[retsize - 1]);

    return ret;
}

char* simple_escape(char* key, char* value) {
    static const char kAssign[] = "='";
    static const char kCloseQuote[] = "'";
    static const char kNestedQuote[] = "'\\''";

    const size_t key_len = strlen(key);
    const size_t val_len = strlen(value);

    size_t overhead = strlen(kAssign) + strlen(kCloseQuote) + 1;

    char* cur = value;
    while (cur = strchr(cur, '\'')) {
        ++cur;
        overhead += strlen(kNestedQuote) - 1;
    }

    const size_t ret_len = key_len + val_len + overhead;
    char* const ret = calloc(ret_len, sizeof(*ret));

    cur = ret;
    cur = mempcpy(cur, key, key_len);
    cur = mempcpy(cur, kAssign, strlen(kAssign));

    for (unsigned i = 0; i < val_len; ++i) {
        const char c = value[i];
        if (c != '\'') {
            cur = mempcpy(cur, &c, sizeof(c));
        } else {
            cur = mempcpy(cur, kNestedQuote, strlen(kNestedQuote));
        }
    }

    cur = mempcpy(cur, kCloseQuote, strlen(kCloseQuote));

    return ret;
}
