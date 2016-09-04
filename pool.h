/******************************************************************************
* pool.h                                                                      *
*                                                                             *
* Description: Event pool manager.                                            *
*                                                                             *
* Author(s):   Longqi Cai (longqic@andrew.cmu.edu)                            *
*                                                                             *
*******************************************************************************/

#ifndef POOL_H
#define POOL_H

#include <sys/select.h>
#include "io.h"

#define MAX_CONNS (FD_SETSIZE - 10)

typedef struct {
  int fd;
  int idx;
  buf_t* buf;
} conn_t;

conn_t* cn_new(int fd, int idx);
void cn_free(conn_t* conn);


typedef struct {
  size_t n_conns;
  conn_t** conns;

  int max_fd;
  size_t n_ready;
  fd_set read_set;
  fd_set write_set;
  fd_set read_ready;
  fd_set write_ready;
} pool_t;

pool_t* pl_new(int sock);
void pl_free(pool_t* p);
void pl_ready(pool_t* p);

int pl_add_conn(pool_t* p, conn_t* c);
int pl_del_conn(pool_t* p, conn_t* c);

#endif // POOL_H
