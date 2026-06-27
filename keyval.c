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
    const size_t len = mbstowcs(NULL, str, 0);
    if (len == ((size_t)-1)) return NULL;

    const size_t size = sizeof(keyval) + sizeof(wchar_t) * (len + 1);

    keyval* const ret = malloc(size);
    if (!ret) return NULL;

    mbstowcs(&ret->key[0], str, len + 1);

    wchar_t* const equal = wcschr(&ret->key[0], L'=');
    if (!equal) { free(ret); return NULL; }
    *equal = L'\0';
    ret->value = &equal[1];

    return ret;
}
