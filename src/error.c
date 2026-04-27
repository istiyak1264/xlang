#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

void error(int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "\033[1;31m[Error]\033[0m line %d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

void warning(int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "\033[1;33m[Warning]\033[0m line %d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}