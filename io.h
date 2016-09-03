/******************************************************************************
* io.h                                                                        *
*                                                                             *
* Description: TODO                                                           *
*                                                                             *
* Author(s):   Longqi Cai (longqic@andrew.cmu.edu)                            *
*                                                                             *
*******************************************************************************/

#ifndef IO_H
#define IO_H

#include <sys/types.h>

#define BUFSZ 4096

typedef struct {
  ssize_t sz;
  void* data;
} buf_t;

buf_t* bf_new();

#endif // IO_H
