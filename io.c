/**
 * @file io.c
 * @brief Implementation of io.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <stdlib.h>
#include "io.h"

buf_t* buf_new() {
  buf_t* buf = malloc(sizeof(buf_t));
  buf->sz = 0;
  buf->data = malloc(BUFSZ+1);
  buf->data_p = buf->data;
  return buf;
}

void buf_free(buf_t* buf) {
  free(buf->data);
  free(buf);
}

void* buf_end(buf_t* buf) {
  return buf->data + buf->sz;
}
