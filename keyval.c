#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "keyval.h"

keyval* keyval_new(const char* str) {
    const size_t len = strlen(str);
    const size_t size = sizeof(keyval) + len + 1;
    keyval* const ret = calloc(size, 1);
    if (ret == NULL) return ret;

    memcpy(ret->key, str, len);
    char* const eq = memchr(ret->key, '=', len);
    *eq = '\0';
    ret->value = &eq[1];

    return ret;
}
