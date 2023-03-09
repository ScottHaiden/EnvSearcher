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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct {
    int arg_index;
    char delim;
} options;

options parse_args(int argc, char** argv) {
    options ret = {.arg_index = 1, .delim = '\n'};

    while (true) {
        const int opt = getopt(argc, argv, "zZ");
        if (opt == -1) break;
        switch (opt) {
            case 'z': ret.delim = '\0'; break;
            case 'Z': ret.delim = '\n'; break;
            case '?': exit(2); break;
        }
    }

    ret.arg_index = optind;
    return ret;
}

int key_matches(const char* haystack, const char* needle, size_t needle_len) {
    const char* const end = strchr(haystack, '=');
    const size_t len = end - haystack;

    return memmem(haystack, len, needle, needle_len) != NULL;
}

int main(int argc, char * argv[], char * envp[]) {
    const options options = parse_args(argc, &argv[0]);

    const int nargs = argc - options.arg_index;
    if (nargs > 1) exit(2);

    char* const needle = (nargs == 1) ? argv[options.arg_index] : "";
    const size_t needle_len = strlen(needle);

    for (char** cur = envp; *cur; ++cur) {
        if (!key_matches(*cur, needle, needle_len)) continue;
        printf("%s%c", *cur, options.delim);
    }

    return EXIT_SUCCESS;
}
