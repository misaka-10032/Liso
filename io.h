/**
 * @file io.h
 * @brief Provides IO related functions.
 */

#ifndef IO_H
#define IO_H

#include <sys/types.h>

// BUFSZ will be allocated for each buffer
#define BUFSZ 8192

typedef struct {
  // the actual size that has actually be taken; <= BUFSZ.
  ssize_t sz;
  // the buffer data
  void* data;
} buf_t;

// create a new buffer
buf_t* bf_new();

#endif // IO_H
