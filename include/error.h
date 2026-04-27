#ifndef XLANG_ERROR_H
#define XLANG_ERROR_H

void error(int line, const char *fmt, ...);
void warning(int line, const char *fmt, ...);

#endif