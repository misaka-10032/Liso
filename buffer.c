/**
 * @file buffer.c
 * @brief Implementation of buffer.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include "buffer.h"
#include "logging.h"
#include "utils.h"

buf_t* buf_new() {
  buf_t* buf = malloc(sizeof(buf_t));
  buf->data = malloc(BUFSZ+1);
  buf->data_p = buf->data;
  *(char*) buf->data_p = 0;
  buf->sz = 0;
  return buf;
}

void buf_free(buf_t* buf) {
  free(buf->data);
  free(buf);
}

void* buf_end(buf_t* buf) {
  return buf->data + buf->sz;
}

ssize_t buf_rsize(buf_t* buf) {
  return buf->data + buf->sz - buf->data_p;
}

void buf_reset(buf_t* buf) {
  buf->data_p = buf->data;
  buf->sz = 0;
}

buf_t* mmbuf_new(int fd, size_t sz) {

  buf_t* mmbuf = malloc(sizeof(buf_t));
  mmbuf->sz = sz;
  mmbuf->data = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);
  mmbuf->data_p = mmbuf->data;

  if (mmbuf->data == MAP_FAILED) {
    log_errln("[mmbuf_new] mmap failed on %d: %s",
              fd, strerror(errno));
    errno = 0;
    free(mmbuf);
    return NULL;
  }

  return mmbuf;
}

void mmbuf_free(buf_t* mmbuf) {

  if (!mmbuf)
    return;

  munmap(mmbuf->data, mmbuf->sz);
  free(mmbuf);
}
