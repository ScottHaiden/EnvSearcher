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
#include <wctype.h>

#include "quote.h"

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define ZTALEN(a) (sizeof(a) / sizeof(*a) - 1)

typedef void(*advance_fn)(char** cur, const char* new, size_t nchar);

static inline void advance_noop(char** cur, const char*, size_t nchar) {
    *cur += nchar;
}

static inline void advance(char** cur, const char* new, size_t nchar) {
    *cur = mempcpy(*cur, new, nchar ? nchar : strlen(new));
}

static inline char* append_hex(advance_fn fn, char* cur, char new) {
    static const char kTable[] = "0123456789abcdef";
    static const char kPrefix[] = "\\x";

    const unsigned byte = new;

    fn(&cur, kPrefix, ZTALEN(kPrefix));
    fn(&cur, &kTable[(byte >> 4) & 0x0f], 1);
    fn(&cur, &kTable[(byte >> 0) & 0x0f], 1);

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
    if (posix_spawnp(&child, "printf", &file_actions, NULL, argv, environ)) {
        DIE("posix_spawnp");
    }
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
        cur = append_hex(&advance, cur, value[i]);
    }

    advance(&cur, kCloseQuote, ZTALEN(kCloseQuote));

    assert(cur == &ret[retsize - 1]);
    return ret;
}

size_t _simple_escape(char* out, size_t len, const char* value) {
    static const char kSingleQuote[] = "'";
    static const char kEscapedQuote[] = "\\'";
    static const char kHexSeqPrefix[] = "$'";

    const size_t val_len = strlen(value);
    const char* end = &value[val_len];

    const advance_fn adv = len ? &advance : &advance_noop;

    char* cur = out;
    adv(&cur, kSingleQuote, ZTALEN(kSingleQuote));

    for (const char* c = value; *c;) {
        const int clen = mblen(c, end - c);

        wchar_t wc;
        const size_t converted = mbstowcs(&wc, c, 1);
        assert(converted == 1 || clen < 0);

        if (wc == L'\'') {
            adv(&cur, kSingleQuote, ZTALEN(kSingleQuote));
            adv(&cur, kEscapedQuote, ZTALEN(kSingleQuote));
            adv(&cur, kSingleQuote, ZTALEN(kSingleQuote));
        } else if (!iswprint(wc) || clen < 0) {
            const size_t max = clen < 0 ? 1 : clen;
            adv(&cur, kSingleQuote, ZTALEN(kSingleQuote));
            adv(&cur, kHexSeqPrefix, ZTALEN(kHexSeqPrefix));
            for (int i = 0; i < max; ++i) {
                cur = append_hex(adv, cur, c[i]);
            }
            adv(&cur, kSingleQuote, ZTALEN(kSingleQuote));
            adv(&cur, kSingleQuote, ZTALEN(kSingleQuote));
        } else {
            adv(&cur, c, clen);
        }

        c += clen < 0 ? 1 : clen;
    }

    adv(&cur, kSingleQuote, ZTALEN(kSingleQuote));

    return cur - out;
}

char* quote_simple_escape(char* key, char* value) {
    static const char kAssign[] = "=";

    const size_t key_len = strlen(key);
    const size_t escaped_val_len = _simple_escape(NULL, 0, value);
    const size_t ret_len = 
        key_len +
        ZTALEN(kAssign) +
        escaped_val_len +
        1;

    char* const ret = calloc(ret_len, sizeof(*ret));
    if (!ret) DIE("calloc");

    char* cur = ret;
    advance(&cur, key, 0);
    advance(&cur, kAssign, ZTALEN(kAssign));
    cur += _simple_escape(cur, escaped_val_len + 1, value);

    assert(cur == &ret[ret_len - 1]);
    return ret;
}

char* quote_name_only(char* key, char* unused_value) { return strdup(key); }
