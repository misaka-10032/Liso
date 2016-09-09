/**
 * @file io.h
 * @brief Provides IO related functions.
 */

#ifndef IO_H
#define IO_H

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

// BUFSZ will be allocated for each buffer
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

// create a new buffer
buf_t* buf_new();
// destroy a buffer
void buf_free(buf_t* buf);
// get the end pointer of the buffer
void* buf_end();

#endif // IO_H
