/**
 * @file utils.h
 * @brief Provides utility functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#define DEBUG 2

#ifndef max
  #define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
  #define min(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef enum { false=0, true } bool;

// strip space chars at beginning and the end
void strstrip(char* str);
// check if a str is number
bool isnum(char* str);
// strncpy with \0 at end
char* strncpy0(char* d, const char* s, size_t n);
// strcpy with \0 at end
char* strcpy0(char* d, const char* s);
// check if str ends with suffix, case insensitive
bool caseendswith(const char* str, const char* suffix);
// check if str starts with prefix
bool strstartswith(const char* str, const char* prefix);

#endif // UTILS_H
