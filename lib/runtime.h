#ifndef XLANG_RUNTIME_H
#define XLANG_RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ast.h>

/* Memory management */
void* xlang_malloc(size_t size);
void* xlang_calloc(size_t count, size_t size);
void* xlang_realloc(void* ptr, size_t new_size);
void  xlang_free(void* ptr);

/* String operations */
typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} XLangString;

XLangString* xlang_string_new(const char* initial);
XLangString* xlang_string_from_char(char c);
void         xlang_string_free(XLangString* str);
XLangString* xlang_string_concat(XLangString* a, XLangString* b);
XLangString* xlang_string_substring(XLangString* str, size_t start, size_t end);
int          xlang_string_compare(XLangString* a, XLangString* b);
int          xlang_string_length(XLangString* str);
char         xlang_string_get(XLangString* str, size_t index);
void         xlang_string_set(XLangString* str, size_t index, char c);

/* Array operations (dynamic arrays) */
typedef struct {
    void** data;
    size_t size;
    size_t capacity;
    VarType element_type;
} XLangArray;

XLangArray* xlang_array_new(VarType element_type);
XLangArray* xlang_array_with_capacity(VarType element_type, size_t capacity);
void        xlang_array_free(XLangArray* arr);
void        xlang_array_push(XLangArray* arr, void* element);
void*       xlang_array_get(XLangArray* arr, size_t index);
void        xlang_array_set(XLangArray* arr, size_t index, void* element);
size_t      xlang_array_size(XLangArray* arr);
void        xlang_array_resize(XLangArray* arr, size_t new_size);

/* 2D Array operations */
typedef struct {
    int** data;
    int rows;
    int cols;
} XLangMatrix;

XLangMatrix* xlang_matrix_new(int rows, int cols);
void         xlang_matrix_free(XLangMatrix* mat);
int          xlang_matrix_get(XLangMatrix* mat, int row, int col);
void         xlang_matrix_set(XLangMatrix* mat, int row, int col, int value);

/* Input/Output operations */
char* xlang_input_string(void);
int   xlang_input_int(void);
double xlang_input_double(void);

void xlang_print(const char* format, ...);
void xlang_print_int(int value);
void xlang_print_float(double value);
void xlang_print_string(const char* value);
void xlang_println(const char* format, ...);

/* Type conversion */
int         xlang_string_to_int(const char* str);
double      xlang_string_to_double(const char* str);
char*       xlang_int_to_string(int value);
char*       xlang_double_to_string(double value);

/* Error handling */
typedef enum {
    XLANG_OK = 0,
    XLANG_ERROR_OUT_OF_MEMORY,
    XLANG_ERROR_OUT_OF_BOUNDS,
    XLANG_ERROR_NULL_POINTER,
    XLANG_ERROR_TYPE_MISMATCH
} XLangError;

XLangError xlang_get_last_error(void);
void       xlang_set_error_handler(void (*handler)(XLangError));
const char* xlang_error_string(XLangError error);

/* Math utilities */
int   xlang_min(int a, int b);
int   xlang_max(int a, int b);
int   xlang_clamp(int value, int min, int max);
int   xlang_abs(int value);
double xlang_pow(double base, double exponent);
double xlang_sqrt(double value);

/* Garbage collection (simple mark-and-sweep) */
typedef struct XLangObject {
    int marked;
    struct XLangObject* next;
} XLangObject;

void xlang_gc_init(void);
void xlang_gc_mark(XLangObject* obj);
void xlang_gc_sweep(void);
void xlang_gc_collect(void);

/* Debug utilities */
void xlang_debug_print_memory_usage(void);
void xlang_debug_print_stack_trace(void);

#endif