/******************************************************************************
* io.h                                                                        *
*                                                                             *
* Description: IO related functions.                                          *
*                                                                             *
* Author(s):   Longqi Cai (longqic@andrew.cmu.edu)                            *
*                                                                             *
*******************************************************************************/

#ifndef IO_H
#define IO_H

#include <sys/types.h>

#define BUFSZ 8192

typedef struct {
  ssize_t sz;
  void* data;
} buf_t;

buf_t* bf_new();

#endif // IO_H
