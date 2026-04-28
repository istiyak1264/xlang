#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static XLangError last_error = XLANG_OK;

static void *xlang_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) { last_error = XLANG_ERROR_OUT_OF_MEMORY; return NULL; }
    return ptr;
}

static void xlang_free(void *ptr) { if (ptr) free(ptr); }

XLangString *xlang_string_new(const char *initial) {
    XLangString *str = (XLangString *)xlang_malloc(sizeof(XLangString));
    if (!str) return NULL;
    size_t len     = initial ? strlen(initial) : 0;
    str->length    = len;
    str->capacity  = len + 1;
    str->data      = (char *)xlang_malloc(str->capacity);
    if (!str->data) { xlang_free(str); return NULL; }
    if (initial) strcpy(str->data, initial);
    else         str->data[0] = '\0';
    return str;
}

void xlang_string_free(XLangString *str) {
    if (str) { if (str->data) xlang_free(str->data); xlang_free(str); }
}

int xlang_string_length(XLangString *str) {
    return str ? (int)str->length : 0;
}

XLangArray *xlang_array_new(int element_type) {
    XLangArray *arr = (XLangArray *)xlang_malloc(sizeof(XLangArray));
    if (!arr) return NULL;
    arr->element_type = element_type;
    arr->size         = 0;
    arr->capacity     = 8;
    arr->data         = (void **)xlang_malloc(arr->capacity * sizeof(void *));
    if (!arr->data) { xlang_free(arr); return NULL; }
    for (size_t i = 0; i < arr->capacity; i++) arr->data[i] = NULL;
    return arr;
}

void xlang_array_free(XLangArray *arr) {
    if (arr) { if (arr->data) xlang_free(arr->data); xlang_free(arr); }
}

void xlang_array_push(XLangArray *arr, void *element) {
    if (!arr) return;
    if (arr->size >= arr->capacity) {
        size_t  new_cap  = arr->capacity * 2;
        void  **new_data = (void **)xlang_malloc(new_cap * sizeof(void *));
        if (!new_data) return;
        memcpy(new_data, arr->data, arr->size * sizeof(void *));
        for (size_t i = arr->size; i < new_cap; i++) new_data[i] = NULL;
        xlang_free(arr->data);
        arr->data     = new_data;
        arr->capacity = new_cap;
    }
    arr->data[arr->size++] = element;
}

void *xlang_array_get(XLangArray *arr, size_t index) {
    if (!arr || index >= arr->size) {
        last_error = XLANG_ERROR_OUT_OF_BOUNDS;
        return NULL;
    }
    return arr->data[index];
}

size_t xlang_array_size(XLangArray *arr) { return arr ? arr->size : 0; }

char *xlang_input_string(void) {
    char *buf = (char *)xlang_malloc(4096);
    if (!buf) return NULL;
    if (fgets(buf, 4096, stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        return buf;
    }
    xlang_free(buf);
    return NULL;
}

int xlang_input_int(void) {
    int v; scanf("%d", &v); return v;
}

double xlang_input_double(void) {
    double v; scanf("%lf", &v); return v;
}

void xlang_print(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void xlang_println(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

XLangError xlang_get_last_error(void) { return last_error; }

const char *xlang_error_string(XLangError error) {
    switch (error) {
        case XLANG_OK:                    return "No error";
        case XLANG_ERROR_OUT_OF_MEMORY:   return "Out of memory";
        case XLANG_ERROR_OUT_OF_BOUNDS:   return "Index out of bounds";
        case XLANG_ERROR_NULL_POINTER:    return "Null pointer dereference";
        default:                          return "Unknown error";
    }
}