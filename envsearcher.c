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

typedef struct {
    char flag;
    const char* help_text;
    void (*action)(options*);
} flag;

static const flag* find_flag(const flag* flags, char key) {
    for (const flag* cur = flags; cur->flag; ++cur) {
        if (cur->flag == key) return cur;
    }

    return NULL;
}

static void options_set_delim_nul(options* o) { o->delim = '\0'; }
static void options_set_delim_newline(options* o) { o->delim = '\n'; }
static void options_set_quote_fn_run_printf(options* o) { o->quote_fn = &run_printf; }
static void options_set_quote_fn_normal(options* o) { o->quote_fn = &normal; }
static void options_set_quote_fn_hex_encode(options* o) { o->quote_fn = &hex_encode; }
static void options_set_quote_fn_simple_escape(options* o) { o->quote_fn = &simple_escape; }

static void show_help(const char* argv0, const flag* flags, int exit_code) {
    const char* begin = argv0;
    if (const char* slash = strrchr(argv0, '/')) begin = slash + 1;

    FILE* const out = (exit_code) ? stderr : stdout;

    fprintf(out, "usage: %s", begin);
    for (const flag* cur = flags; cur->flag; ++cur) {
        fprintf(out, " [-%c]", cur->flag);
    }

    fprintf(out, "\n");
    fprintf(out, "\n");
    fprintf(out, "options:\n");

    for (const flag* cur = flags; cur->flag; ++cur) {
        fprintf(out, "  -%c: %s\n", cur->flag, cur->help_text);
    }

    exit(exit_code);
}

static options parse_args(int argc, char** argv) {
    options ret = {
        .arg_index = 1,
        .delim = '\n',
        .quote_fn = &simple_escape,
    };

    const flag flags[] = {
        {
            .flag = 'z', .action = &options_set_delim_nul,
            .help_text = "Use nul char to delimit entries.",
        },
        {
            .flag = 'Z', .action = &options_set_delim_newline,
            .help_text = "Use newline to delimit entries.",
        },
        {
            .flag = 'q', .action = &options_set_quote_fn_run_printf,
            .help_text = "Use printf %q to quote entries. Requires supported printf program.",
        },
        {
            .flag = 'n', .action = &options_set_quote_fn_normal,
            .help_text = "Print values as-is, no quoting.",
        },
        {
            .flag = 'x', .action = &options_set_quote_fn_hex_encode,
            .help_text = "Hex-escape values.",
        },
        {
            .flag = 's', .action = &options_set_quote_fn_simple_escape,
            .help_text = "Use simple escape (default).",
        },
        {},
    };

    while (true) {
        const int opt = getopt(argc, argv, "zZqnxsh");
        if (opt == -1) break;
        if (opt == 'h') show_help(argv[0], &flags[0], 0);

        const flag* flag = find_flag(&flags[0], opt);
        if (flag == NULL) show_help(argv[0], &flags[0], 1);

        flag->action(&ret);
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
