#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ast.h>

/* Global error state */
static XLangError last_error = XLANG_OK;
static void (*error_handler)(XLangError) = NULL;

/* Memory tracking for debugging */
#ifdef XLANG_DEBUG_MEMORY
static size_t total_allocations = 0;
static size_t total_frees = 0;
static size_t current_memory = 0;
#endif

/* ==================== Memory Management ==================== */

void* xlang_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        last_error = XLANG_ERROR_OUT_OF_MEMORY;
        if (error_handler) error_handler(last_error);
        return NULL;
    }
#ifdef XLANG_DEBUG_MEMORY
    total_allocations++;
    current_memory += size;
#endif
    return ptr;
}

void* xlang_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr) {
        last_error = XLANG_ERROR_OUT_OF_MEMORY;
        if (error_handler) error_handler(last_error);
        return NULL;
    }
#ifdef XLANG_DEBUG_MEMORY
    total_allocations++;
    current_memory += count * size;
#endif
    return ptr;
}

void* xlang_realloc(void* ptr, size_t new_size) {
    void* new_ptr = realloc(ptr, new_size);
    if (!new_ptr && new_size > 0) {
        last_error = XLANG_ERROR_OUT_OF_MEMORY;
        if (error_handler) error_handler(last_error);
        return NULL;
    }
    return new_ptr;
}

void xlang_free(void* ptr) {
    if (ptr) {
        free(ptr);
#ifdef XLANG_DEBUG_MEMORY
        total_frees++;
        /* current_memory would need to know size, simplified */
#endif
    }
}

/* ==================== String Operations ==================== */

XLangString* xlang_string_new(const char* initial) {
    XLangString* str = (XLangString*)xlang_malloc(sizeof(XLangString));
    if (!str) return NULL;
    
    size_t len = initial ? strlen(initial) : 0;
    str->length = len;
    str->capacity = len + 1;
    str->data = (char*)xlang_malloc(str->capacity);
    
    if (!str->data) {
        xlang_free(str);
        return NULL;
    }
    
    if (initial) {
        strcpy(str->data, initial);
    } else {
        str->data[0] = '\0';
    }
    
    return str;
}

XLangString* xlang_string_from_char(char c) {
    char buf[2] = {c, '\0'};
    return xlang_string_new(buf);
}

void xlang_string_free(XLangString* str) {
    if (str) {
        if (str->data) xlang_free(str->data);
        xlang_free(str);
    }
}

XLangString* xlang_string_concat(XLangString* a, XLangString* b) {
    if (!a || !b) {
        last_error = XLANG_ERROR_NULL_POINTER;
        return NULL;
    }
    
    size_t new_len = a->length + b->length;
    XLangString* result = xlang_string_new(NULL);
    if (!result) return NULL;
    
    if (new_len + 1 > result->capacity) {
        result->capacity = new_len + 1;
        result->data = (char*)xlang_realloc(result->data, result->capacity);
        if (!result->data) {
            xlang_free(result);
            return NULL;
        }
    }
    
    strcpy(result->data, a->data);
    strcat(result->data, b->data);
    result->length = new_len;
    
    return result;
}

XLangString* xlang_string_substring(XLangString* str, size_t start, size_t end) {
    if (!str) {
        last_error = XLANG_ERROR_NULL_POINTER;
        return NULL;
    }
    
    if (start >= str->length || end > str->length || start >= end) {
        last_error = XLANG_ERROR_OUT_OF_BOUNDS;
        return NULL;
    }
    
    size_t len = end - start;
    char* substr = (char*)xlang_malloc(len + 1);
    if (!substr) return NULL;
    
    strncpy(substr, str->data + start, len);
    substr[len] = '\0';
    
    XLangString* result = xlang_string_new(substr);
    xlang_free(substr);
    return result;
}

int xlang_string_compare(XLangString* a, XLangString* b) {
    if (!a || !b) return -2;
    return strcmp(a->data, b->data);
}

int xlang_string_length(XLangString* str) {
    return str ? str->length : 0;
}

char xlang_string_get(XLangString* str, size_t index) {
    if (!str || index >= str->length) return '\0';
    return str->data[index];
}

void xlang_string_set(XLangString* str, size_t index, char c) {
    if (!str || index >= str->length) return;
    str->data[index] = c;
}

/* ==================== Array Operations ==================== */

XLangArray* xlang_array_new(VarType element_type) {
    return xlang_array_with_capacity(element_type, 8);
}

XLangArray* xlang_array_with_capacity(VarType element_type, size_t capacity) {
    XLangArray* arr = (XLangArray*)xlang_malloc(sizeof(XLangArray));
    if (!arr) return NULL;
    
    arr->element_type = element_type;
    arr->size = 0;
    arr->capacity = capacity;
    arr->data = (void**)xlang_calloc(capacity, sizeof(void*));
    
    if (!arr->data && capacity > 0) {
        xlang_free(arr);
        return NULL;
    }
    
    return arr;
}

void xlang_array_free(XLangArray* arr) {
    if (arr) {
        if (arr->data) {
            /* Free individual elements if they're pointers */
            for (size_t i = 0; i < arr->size; i++) {
                if (arr->data[i]) {
                    /* Check if element is a string or other heap-allocated type */
                    /* For simplicity, we assume caller manages element memory */
                }
            }
            xlang_free(arr->data);
        }
        xlang_free(arr);
    }
}

void xlang_array_push(XLangArray* arr, void* element) {
    if (!arr) return;
    
    if (arr->size >= arr->capacity) {
        size_t new_capacity = arr->capacity * 2;
        if (new_capacity == 0) new_capacity = 8;
        
        void** new_data = (void**)xlang_realloc(arr->data, new_capacity * sizeof(void*));
        if (!new_data) return;
        
        arr->data = new_data;
        arr->capacity = new_capacity;
    }
    
    arr->data[arr->size++] = element;
}

void* xlang_array_get(XLangArray* arr, size_t index) {
    if (!arr || index >= arr->size) {
        last_error = XLANG_ERROR_OUT_OF_BOUNDS;
        return NULL;
    }
    return arr->data[index];
}

void xlang_array_set(XLangArray* arr, size_t index, void* element) {
    if (!arr || index >= arr->size) {
        last_error = XLANG_ERROR_OUT_OF_BOUNDS;
        return;
    }
    arr->data[index] = element;
}

size_t xlang_array_size(XLangArray* arr) {
    return arr ? arr->size : 0;
}

void xlang_array_resize(XLangArray* arr, size_t new_size) {
    if (!arr) return;
    
    if (new_size > arr->capacity) {
        void** new_data = (void**)xlang_realloc(arr->data, new_size * sizeof(void*));
        if (!new_data) return;
        
        arr->data = new_data;
        arr->capacity = new_size;
    }
    
    /* Initialize new elements to NULL */
    for (size_t i = arr->size; i < new_size; i++) {
        arr->data[i] = NULL;
    }
    
    arr->size = new_size;
}

/* ==================== 2D Array Operations ==================== */

XLangMatrix* xlang_matrix_new(int rows, int cols) {
    XLangMatrix* mat = (XLangMatrix*)xlang_malloc(sizeof(XLangMatrix));
    if (!mat) return NULL;
    
    mat->rows = rows;
    mat->cols = cols;
    mat->data = (int**)xlang_malloc(rows * sizeof(int*));
    
    if (!mat->data) {
        xlang_free(mat);
        return NULL;
    }
    
    for (int i = 0; i < rows; i++) {
        mat->data[i] = (int*)xlang_calloc(cols, sizeof(int));
        if (!mat->data[i]) {
            for (int j = 0; j < i; j++) xlang_free(mat->data[j]);
            xlang_free(mat->data);
            xlang_free(mat);
            return NULL;
        }
    }
    
    return mat;
}

void xlang_matrix_free(XLangMatrix* mat) {
    if (mat) {
        if (mat->data) {
            for (int i = 0; i < mat->rows; i++) {
                if (mat->data[i]) xlang_free(mat->data[i]);
            }
            xlang_free(mat->data);
        }
        xlang_free(mat);
    }
}

int xlang_matrix_get(XLangMatrix* mat, int row, int col) {
    if (!mat || row < 0 || row >= mat->rows || col < 0 || col >= mat->cols) {
        last_error = XLANG_ERROR_OUT_OF_BOUNDS;
        return 0;
    }
    return mat->data[row][col];
}

void xlang_matrix_set(XLangMatrix* mat, int row, int col, int value) {
    if (!mat || row < 0 || row >= mat->rows || col < 0 || col >= mat->cols) {
        last_error = XLANG_ERROR_OUT_OF_BOUNDS;
        return;
    }
    mat->data[row][col] = value;
}

/* ==================== Input/Output Operations ==================== */

char* xlang_input_string(void) {
    char* buffer = (char*)xlang_malloc(4096);
    if (!buffer) return NULL;
    
    if (fgets(buffer, 4096, stdin)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        return buffer;
    }
    
    xlang_free(buffer);
    return NULL;
}

int xlang_input_int(void) {
    int value;
    scanf("%d", &value);
    return value;
}

double xlang_input_double(void) {
    double value;
    scanf("%lf", &value);
    return value;
}

void xlang_print(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void xlang_print_int(int value) {
    printf("%d", value);
}

void xlang_print_float(double value) {
    printf("%g", value);
}

void xlang_print_string(const char* value) {
    printf("%s", value);
}

void xlang_println(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

/* ==================== Type Conversion ==================== */

int xlang_string_to_int(const char* str) {
    if (!str) return 0;
    return atoi(str);
}

double xlang_string_to_double(const char* str) {
    if (!str) return 0.0;
    return atof(str);
}

char* xlang_int_to_string(int value) {
    char* buffer = (char*)xlang_malloc(32);
    if (!buffer) return NULL;
    sprintf(buffer, "%d", value);
    return buffer;
}

char* xlang_double_to_string(double value) {
    char* buffer = (char*)xlang_malloc(64);
    if (!buffer) return NULL;
    sprintf(buffer, "%g", value);
    return buffer;
}

/* ==================== Error Handling ==================== */

XLangError xlang_get_last_error(void) {
    return last_error;
}

void xlang_set_error_handler(void (*handler)(XLangError)) {
    error_handler = handler;
}

const char* xlang_error_string(XLangError error) {
    switch (error) {
        case XLANG_OK: return "No error";
        case XLANG_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case XLANG_ERROR_OUT_OF_BOUNDS: return "Index out of bounds";
        case XLANG_ERROR_NULL_POINTER: return "Null pointer dereference";
        case XLANG_ERROR_TYPE_MISMATCH: return "Type mismatch";
        default: return "Unknown error";
    }
}

/* ==================== Math Utilities ==================== */

int xlang_min(int a, int b) { return a < b ? a : b; }
int xlang_max(int a, int b) { return a > b ? a : b; }
int xlang_clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
int xlang_abs(int value) { return value < 0 ? -value : value; }
double xlang_pow(double base, double exponent) { return pow(base, exponent); }
double xlang_sqrt(double value) { return sqrt(value); }

/* ==================== Garbage Collection ==================== */

static XLangObject* gc_head = NULL;

void xlang_gc_init(void) {
    gc_head = NULL;
}

void xlang_gc_mark(XLangObject* obj) {
    if (!obj || obj->marked) return;
    obj->marked = 1;
}

void xlang_gc_sweep(void) {
    XLangObject** current = &gc_head;
    while (*current) {
        if (!(*current)->marked) {
            XLangObject* unreached = *current;
            *current = unreached->next;
            xlang_free(unreached);
        } else {
            (*current)->marked = 0;
            current = &(*current)->next;
        }
    }
}

void xlang_gc_collect(void) {
    /* Mark phase - would need to mark all reachable objects */
    /* Sweep phase */
    xlang_gc_sweep();
}

/* ==================== Debug Utilities ==================== */

void xlang_debug_print_memory_usage(void) {
#ifdef XLANG_DEBUG_MEMORY
    printf("Memory Statistics:\n");
    printf("  Total allocations: %zu\n", total_allocations);
    printf("  Total frees: %zu\n", total_frees);
    printf("  Current memory: %zu bytes\n", current_memory);
    printf("  Leaks: %zu\n", total_allocations - total_frees);
#else
    printf("Compile with -DXLANG_DEBUG_MEMORY for memory statistics\n");
#endif
}

void xlang_debug_print_stack_trace(void) {
    /* This would require platform-specific code */
    printf("Stack trace not implemented for this platform\n");
}