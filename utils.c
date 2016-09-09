/**
 * @file utils.c
 * @brief Implementation of utils.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <string.h>
#include <stdlib.h>
#include "utils.h"

void strstrip(char* str) {
  char* p;
  for (p = str; isspace(*p); p++);
  if (*p == 0) {
    str[0] = 0;  // all spaces
    return;
  }

  char* q;
  for (q = str+strlen(str)-1;
       isspace(*q) && q > p; q--);
  *++q = 0;

  char *s, *r;
  for (s = str, r = p; r < q; *s++ = *r++);
  *s = 0;
}

bool isnum(char* str) {
  char* p;
  for (p = str; *p; p++)
    if (!isdigit(*p))
      return false;
  return true;
}

char* strncpy0(char* d, const char* s, size_t n) {
  d[0] = 0;
  return strncat(d, s, n);
}

char* strcpy0(char* d, const char* s) {
  d[0] = 0;
  return strcat(d, s);
}


