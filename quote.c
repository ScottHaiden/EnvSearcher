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

static inline wchar_t* serialize(wchar_t* cur, const wchar_t* new, size_t nchar) {
    return mempcpy(cur, new, (nchar ? nchar : wcslen(new)) * sizeof(*cur));
}

wchar_t* run_printf(wchar_t* key, wchar_t* value) {
    const int fd = memfd_create("output", 0);
    if (fd < 0) DIE("memfd_create");

    const size_t mb_keylen = wcstombs(NULL, key, 0);
    const size_t mb_vallen = wcstombs(NULL, value, 0);

    if (!mb_keylen || !mb_vallen) return NULL;

    char mb_key[mb_keylen + 1];
    char mb_val[mb_vallen + 1];

    wcstombs(mb_key, key, mb_keylen + 1);
    wcstombs(mb_val, value, mb_vallen + 1);

    pid_t child = 0;
    char* argv[] = {"printf", "%s=%q", mb_key, mb_val, NULL};
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

    const size_t len = mbstowcs(NULL, result, 0);
    if (!len) DIE("mbstowcs printf result");

    wchar_t* const ret = calloc(len + 1, sizeof(*ret));
    mbstowcs(ret, result, len + 1);

    free(result);

    return ret;
}

wchar_t* normal(wchar_t* key, wchar_t* value) {
    static const wchar_t kEquals[] = L"=";
    const size_t eq_len = wcslen(kEquals);

    const size_t key_len = wcslen(key);
    const size_t val_len = wcslen(value);

    const size_t ret_len = key_len + eq_len + val_len + 1;
    wchar_t* ret = calloc(ret_len, sizeof(*ret));

    wchar_t* cur = ret;
    cur = serialize(cur, key, 0);
    cur = serialize(cur, kEquals, 0);
    cur = serialize(cur, value, 0);

    return ret;
}

wchar_t* hex_encode(wchar_t* key, wchar_t* value) {
    static const wchar_t kTable[] = L"0123456789abcdef";

    static const wchar_t kAssignment[] = L"=$'";
    static const wchar_t kHexPrefix[] = L"\\x";
    static const wchar_t kCloseQuote[] = L"'";

    const size_t overhead = wcslen(key) + wcslen(kAssignment) + wcslen(kCloseQuote);

    const size_t valbytes = wcstombs(NULL, value, 0);
    if (!valbytes) return NULL;
    char val_mb[valbytes + 1];
    wcstombs(val_mb, value, sizeof(val_mb));

    const size_t retsize = overhead + valbytes * 4 + 1;
    wchar_t* const ret = calloc(retsize, sizeof(*ret));

    wchar_t* cur = ret;
    cur = serialize(cur, key, 0);
    cur = serialize(cur, kAssignment, 0);

    for (unsigned i = 0; i < valbytes; ++i) {
        const unsigned byte = val_mb[i];
        cur = serialize(cur, kHexPrefix, 0);
        cur = serialize(cur, &kTable[(byte >> 4) & 0x0f], 1);
        cur = serialize(cur, &kTable[(byte >> 0) & 0x0f], 1);
    }

    cur = serialize(cur, kCloseQuote, 0);

    assert(cur == &ret[retsize - 1]);

    return ret;
}

wchar_t* simple_escape(wchar_t* key, wchar_t* value) {
    static const wchar_t kAssign[] = L"='";
    static const wchar_t kCloseQuote[] = L"'";
    static const wchar_t kNestedQuote[] = L"'\\''";

    const size_t key_len = wcslen(key);
    const size_t val_len = wcslen(value);

    size_t overhead = wcslen(kAssign) + wcslen(kCloseQuote) + 1;

    wchar_t* cur = value;
    while ((cur = wcschr(cur, L'\''))) {
        ++cur;
        overhead += wcslen(kNestedQuote) - 1;
    }

    const size_t ret_len = key_len + val_len + overhead;
    wchar_t* const ret = calloc(ret_len, sizeof(*ret));

    cur = ret;
    cur = serialize(cur, key, 0);
    cur = serialize(cur, kAssign, 0);

    for (unsigned i = 0; i < val_len; ++i) {
        const wchar_t c = value[i];
        if (c != L'\'') {
            cur = serialize(cur, &c, 1);
        } else {
            cur = serialize(cur, kNestedQuote, 0);
        }
    }

    cur = serialize(cur, kCloseQuote, 0);

    return ret;
}

wchar_t* name_only(wchar_t* key, wchar_t* unused_value) { return wcsdup(key); }
