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
#include <ctype.h>

#include "quote.h"

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define ZTALEN(a) (sizeof(a) / sizeof(*a) - 1)

static inline void advance(char** cur, const char* new, size_t nchar) {
    *cur = mempcpy(*cur, new, nchar ? nchar : strlen(new));
}

static inline char* append_hex(char* cur, char new) {
    static const char kTable[] = "0123456789abcdef";
    static const char kPrefix[] = "\\x";

    const unsigned byte = new;

    advance(&cur, kPrefix, ZTALEN(kPrefix));
    advance(&cur, &kTable[(byte >> 4) & 0x0f], 1);
    advance(&cur, &kTable[(byte >> 0) & 0x0f], 1);

    return cur;
}

char* quote_run_printf(char* key, char* value) {
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

    char* const result = calloc(flen + 1, sizeof(*result));
    if (!result) DIE("calloc");

    if (lseek(fd, 0, SEEK_SET)) DIE("lseek");
    const ssize_t amount = read(fd, result, flen);
    if (amount < 0) DIE("read");
    assert(((size_t)(amount)) == flen);

    if (close(fd)) DIE("close");

    return result;
}

char* quote_normal(char* key, char* value) {
    static const char kEquals[] = "=";
    const size_t eq_len = ZTALEN(kEquals);

    const size_t key_len = strlen(key);
    const size_t val_len = strlen(value);

    const size_t ret_len = key_len + eq_len + val_len + 1;
    char* ret = calloc(ret_len, sizeof(*ret));

    char* cur = ret;
    advance(&cur, key, 0);
    advance(&cur, kEquals, ZTALEN(kEquals));
    advance(&cur, value, 0);

    assert(cur == &ret[ret_len - 1]);
    return ret;
}

char* quote_hex_encode(char* key, char* value) {
    static const char kTable[] = "0123456789abcdef";

    static const char kAssignment[] = "=$'";
    static const char kCloseQuote[] = "'";

    const size_t key_len = strlen(key);
    const size_t val_len = strlen(value);

    const size_t overhead = key_len + ZTALEN(kAssignment) + ZTALEN(kCloseQuote);

    const size_t retsize = overhead + val_len * 4 + 1;
    char* const ret = calloc(retsize, sizeof(*ret));

    char* cur = ret;
    advance(&cur, key, 0);
    advance(&cur, kAssignment, ZTALEN(kAssignment));

    for (unsigned i = 0; i < val_len; ++i) {
        cur = append_hex(cur, value[i]);
    }

    advance(&cur, kCloseQuote, ZTALEN(kCloseQuote));

    assert(cur == &ret[retsize - 1]);
    return ret;
}

char* quote_simple_escape(char* key, char* value) {
    static const char kAssign[] = "='";
    static const char kSingleQuote[] = "'";
    static const char kEscapedQuote[] = "\\'";
    static const char kHexSeqPrefix[] = "$'";

    const size_t key_len = strlen(key);

    size_t overhead = ZTALEN(kAssign) + ZTALEN(kSingleQuote) + 1;

    size_t enc_len = 0;
    for (const char* cur = value; *cur; ++cur) {
        if (*cur == '\'') {
            enc_len +=
                ZTALEN(kSingleQuote) +
                ZTALEN(kEscapedQuote) +
                ZTALEN(kSingleQuote);
        } else if (!isprint(*cur)) {
            enc_len +=
                ZTALEN(kSingleQuote) +  // close previous sequence
                ZTALEN(kHexSeqPrefix) + // open hex sequence
                4 +                     // \x00 for  the byte
                ZTALEN(kSingleQuote) +  // close hex sequence
                ZTALEN(kSingleQuote);   // open normal sequence
        } else {
            ++enc_len;
        }
    }

    const size_t ret_len = key_len + enc_len + overhead;
    char* const ret = calloc(ret_len, sizeof(*ret));

    char* cur = ret;
    advance(&cur, key, 0);
    advance(&cur, kAssign, ZTALEN(kAssign));

    for (const char* c = &value[0]; *c; ++c) {
        if (*c == '\'') {
            advance(&cur, kSingleQuote, ZTALEN(kSingleQuote));
            advance(&cur, kEscapedQuote, ZTALEN(kEscapedQuote));
            advance(&cur, kSingleQuote, ZTALEN(kSingleQuote));
        } else if (!isprint(*c)) {
            advance(&cur, kSingleQuote, ZTALEN(kSingleQuote));
            advance(&cur, kHexSeqPrefix, ZTALEN(kHexSeqPrefix));
            cur = append_hex(cur, *c);
            advance(&cur, kSingleQuote, ZTALEN(kSingleQuote));
            advance(&cur, kSingleQuote, ZTALEN(kSingleQuote));
        } else {
            advance(&cur, c, 1);
        }
    }

    advance(&cur, kSingleQuote, ZTALEN(kSingleQuote));

    assert(cur == &ret[ret_len - 1]);
    return ret;
}

char* quote_name_only(char* key, char* unused_value) { return strdup(key); }
