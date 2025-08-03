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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "keyval.h"
#include "quote.h"

typedef struct {
    int arg_index;
    char delim;
    char* (*quote_fn)(char*, char*);
} options;

static options parse_args(int argc, char** argv) {
    options ret = {
        .arg_index = 1,
        .delim = '\n',
        .quote_fn = &normal,
    };

    while (true) {
        const int opt = getopt(argc, argv, "zZqnxs");
        if (opt == -1) break;
        switch (opt) {
            case 'z': ret.delim = '\0';              break;
            case 'Z': ret.delim = '\n';              break;
            case 'q': ret.quote_fn = &run_printf;    break;
            case 'n': ret.quote_fn = &normal;        break;
            case 'x': ret.quote_fn = &hex_encode;    break;
            case 's': ret.quote_fn = &simple_escape; break;
            case '?': exit(2);                       break;
        }
    }

    ret.arg_index = optind;
    return ret;
}

static int compare(const void* a, const void* b) {
    const char* const* str_a = a;
    const char* const* str_b = b;

    return strcmp(*str_a, *str_b);
}

static void sort_env(char** envp) {
    char** cur = envp;
    while (*cur) ++cur;

    return qsort(envp, cur - envp, sizeof(*envp), &compare);
}

int main(int argc, char * argv[], char * envp[]) {
    const options options = parse_args(argc, &argv[0]);

    const int nargs = argc - options.arg_index;
    if (nargs > 1) exit(2);

    char* const needle = (nargs == 1) ? argv[options.arg_index] : "";
    const size_t needle_len = strlen(needle);

    sort_env(envp);

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
