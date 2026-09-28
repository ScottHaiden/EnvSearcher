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

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "keyval.h"

keyval* keyval_new(const char* str) {
    const size_t len = strlen(str);
    const size_t size = sizeof(keyval) + len + 1;

    keyval* const ret = calloc(size, 1);
    if (!ret) return NULL;

    memcpy(&ret->key, str, len);
    char* const equal = strchr(ret->key, '=');
    if (!equal) { free(ret); return NULL; }
    *equal = '\0';
    ret->value = &equal[1];
    ret->key_len = equal - &ret->key[0];
    ret->val_len = len - ret->key_len - 1;
    return ret;
}
