/**
 * @file buffer.h
 * @brief Provides buffering to accommodate different bandwidth.
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

// buf capacity
#define BUFSZ 8192
// end of line
#define CRLF "\r\n"

typedef struct {
  // the actual size that has actually be taken; <= BUFSZ.
  ssize_t sz;
  // the start of buffer data
  void* data;
  // pointer to the current position in data
  void* data_p;
} buf_t;

// constructor
buf_t* buf_new();
// destructor
void buf_free(buf_t* buf);
// get the end pointer of the buffer
void* buf_end(buf_t* buf);
// get the remain size from data_p to sz
ssize_t buf_rsize(buf_t* buf);
// reset data_p
void buf_reset();

// constructor for memory-mapped buffer
buf_t* mmbuf_new(int fd, size_t sz);
// destroy a mmbuf
void mmbuf_free(buf_t* buf);

#endif // BUFFER_H
