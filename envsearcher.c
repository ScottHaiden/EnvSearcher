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
#include <locale.h>

#include "keyval.h"
#include "quote.h"

#define DIE(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct {
    int arg_index;
    char delim;
    char* (*quote_fn)(char*, char*);
} options;

typedef enum {
    JOB_UNSET = 0,
    JOB_QUOTE = 1,
    JOB_DELIM = 2,
} job_e;

typedef struct {
    char flag;
    const char* help_text;
    job_e type;
    union {
        char* (*quote_fn)(char*, char*);
        char delim;
    } value;
} flag;

static const flag* find_flag(const flag* flags, char key) {
    for (const flag* cur = flags; cur->flag; ++cur) {
        if (cur->flag == key) return cur;
    }

    return NULL;
}

[[noreturn]] static void show_help(const char* argv0, const flag* flags, int exit_code) {
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
        .quote_fn = &quote_simple_escape,
    };

    const flag flags[] = {
        {
            .flag = 'N',
            .type = JOB_QUOTE, .value.quote_fn = &quote_name_only,
            .help_text = "Print only the name of the variables.",
        },
        {
            .flag = 'n',
            .type = JOB_QUOTE, .value.quote_fn = &quote_normal,
            .help_text = "Print values as-is, no quoting.",
        },
        {
            .flag = 'q',
            .type = JOB_QUOTE, .value.quote_fn = &quote_run_printf,
            .help_text = "Use printf %q to quote entries. Requires supported printf program.",
        },
        {
            .flag = 's',
            .type = JOB_QUOTE, .value.quote_fn = &quote_simple_escape,
            .help_text = "Use simple escape (default).",
        },
        {
            .flag = 'x',
            .type = JOB_QUOTE, .value.quote_fn = &quote_hex_encode,
            .help_text = "Hex-escape values.",
        },
        {
            .flag = 'Z',
            .type = JOB_DELIM, .value.delim = '\n',
            .help_text = "Use newline to delimit entries.",
        },
        {
            .flag = 'z',
            .type = JOB_DELIM, .value.delim = '\0',
            .help_text = "Use nul char to delimit entries.",
        },
        {
            .flag = 'h',
            .type = JOB_UNSET,
            .help_text = "Show this help.",
        },
        {},
    };

    char opts[sizeof(flags) / sizeof(*flags) + 1];
    for (unsigned i = 0; true; ++i) {
        if (!(opts[i] = flags[i].flag)) break;
    }

    while (true) {
        const int opt = getopt(argc, argv, opts);
        if (opt == -1) break;

        const flag* flag = find_flag(&flags[0], opt);
        if (flag == NULL) show_help(argv[0], &flags[0], 1);

        switch (flag->type) {
            case JOB_UNSET: show_help(argv[0], &flags[0], 0);    break;
            case JOB_QUOTE: ret.quote_fn = flag->value.quote_fn; break;
            case JOB_DELIM: ret.delim = flag->value.delim;       break;
            default: show_help(argv[0], &flags[0], 1);           break;
        }
    }

    ret.arg_index = optind;
    return ret;
}

static int compare(const void* a, const void* b) {
    const char* const* str_a = a;
    const char* const* str_b = b;

    return strcasecmp(*str_a, *str_b);
}

static void sort_env(char** envp) {
    char** cur = envp;
    while (*cur) ++cur;

    return qsort(envp, cur - envp, sizeof(*envp), &compare);
}

int main(int argc, char * argv[], char * envp[]) {
    setlocale(LC_ALL, "");

    const options options = parse_args(argc, &argv[0]);

    const int nargs = argc - options.arg_index;
    if (nargs > 1) exit(2);

    char* const needle = argv[options.arg_index] ? argv[options.arg_index] : "";
    const size_t needle_len = strlen(needle);

    sort_env(envp);

    bool any_matched = false;

    for (char** cur = envp; *cur; ++cur) {
        keyval* const kv = keyval_new(*cur);
        if (!kv) continue;

        if (!needle || memmem(kv->key, kv->key_len, needle, needle_len)) {
            any_matched = true;
            char* const message = options.quote_fn(kv->key, kv->value);
            printf("%s%c", message, options.delim);
            free(message);
        }

        free(kv);
    }

    return any_matched ? EXIT_SUCCESS : EXIT_FAILURE;
}
