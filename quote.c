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
    static const char kTable[] = 
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f"
        "303132333435363738393a3b3c3d3e3f"
        "404142434445464748494a4b4c4d4e4f"
        "505152535455565758595a5b5c5d5e5f"
        "606162636465666768696a6b6c6d6e6f"
        "707172737475767778797a7b7c7d7e7f"
        "808182838485868788898a8b8c8d8e8f"
        "909192939495969798999a9b9c9d9e9f"
        "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
        "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
        "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
        "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
        "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
        "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

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
        const char c = value[i];
        const size_t index = c * 2;

        cur = mempcpy(cur, kHexPrefix, strlen(kHexPrefix));
        cur = mempcpy(cur, &kTable[index], 2);
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
