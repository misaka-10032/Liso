/**
 * @file test_driver.c
 * @brief Test driver for utility functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <assert.h>
#include <string.h>
#include "utils.h"


bool _test_strstrip(char* str, char* tgt) {
  char s[128] = {0};
  strcat(s, str);
  strstrip(s);
  return !strcmp(tgt, s);
}

void test_strstrip() {
  assert(_test_strstrip(" abc  ", "abc"));
  assert(_test_strstrip("\t\rabc\n", "abc"));
  assert(_test_strstrip("\t\r\n", ""));
}

void test_isnum() {
  assert(isnum("1234567890"));
  assert(isnum(""));
  assert(!isnum("92x"));
}

void test_strstartswith() {
  assert(strstartswith("abc", "a"));
  assert(!strstartswith("b", "bc"));
  assert(!strstartswith("bc", "cd"));
}

int main() {
  test_strstrip();
  test_isnum();
  test_strstartswith();
  printf("[test_driver] Passed!\n");
  return 0;
}
