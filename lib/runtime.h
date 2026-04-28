#ifndef XLANG_RUNTIME_H
#define XLANG_RUNTIME_H

#include "ast.h"      /* VarType lives here – single definition */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ---------------------------------------------------------------
 * String
 * --------------------------------------------------------------- */
typedef struct {
    char   *data;
    size_t  length;
    size_t  capacity;
} XLangString;

XLangString *xlang_string_new(const char *initial);
void         xlang_string_free(XLangString *str);
int          xlang_string_length(XLangString *str);

/* ---------------------------------------------------------------
 * Array
 * --------------------------------------------------------------- */
typedef struct {
    void  **data;
    size_t  size;
    size_t  capacity;
    int     element_type;
} XLangArray;

XLangArray *array_new(int element_type);
void        array_free(XLangArray *arr);
void        array_push(XLangArray *arr, void *element);
void       *array_get(XLangArray *arr, size_t index);
size_t      array_size(XLangArray *arr);

/* ---------------------------------------------------------------
 * I/O
 * --------------------------------------------------------------- */
char  *string_input(void);
int    int_input(void);
double double_input(void);
void   xlang_print(const char *format, ...);
void   xlang_println(const char *format, ...);

/* ---------------------------------------------------------------
 * Error
 * --------------------------------------------------------------- */
typedef enum {
    XLANG_OK = 0,
    XLANG_ERROR_OUT_OF_MEMORY,
    XLANG_ERROR_OUT_OF_BOUNDS,
    XLANG_ERROR_NULL_POINTER
} XLangError;

XLangError  get_last_error(void);
const char *error_string(XLangError error);

#endif