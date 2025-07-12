#pragma once

typedef struct {
    char* value;
    char key[];
} keyval;

keyval* keyval_new(const char* str);
