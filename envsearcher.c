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
