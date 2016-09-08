/**
 * @file io.c
 * @brief Implementation of io.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <stdlib.h>
#include "io.h"

buf_t* bf_new() {
  buf_t* buf = malloc(sizeof(buf_t));
  buf->data = malloc(BUFSZ);
  return buf;
}

void bf_free(buf_t* buf) {
  free(buf->data);
  free(buf);
}
