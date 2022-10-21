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

#define _XOPEN_SOURCE 700

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int key_matches(const char* haystack, const char* needle) {
    const char* const end = strchr(haystack, '=');
    const size_t len = end - haystack;

    char buf[len + 1];
    memcpy(buf, haystack, len);
    buf[len] = '\0';
    return strstr(buf, needle) != NULL;
}

int main(int argc, char * argv[], char * envp[]) {
    char* needle = (argc > 1) ? argv[1] : "";

    for (char** cur = envp; *cur; ++cur) {
        if (key_matches(*cur, needle)) puts(*cur);
    }

    return EXIT_SUCCESS;
}
