/**
 * @file pool.h
 * @brief Event pool manager.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 *
 * This module manages a pool of connections. It allocates or deallocates
 * buffers, and manages connection in a list, but do NOT open or close
 * sockets.
 */

#ifndef POOL_H
#define POOL_H

#include <sys/select.h>
#include "request.h"
#include "response.h"
#include "io.h"

#define MAX_CONNS (FD_SETSIZE - 10)

/* conn_t */
typedef struct {
  // File desriptor for the client socket
  int fd;
  // Idx in the pool
  int idx;
  // Parsed request header
  req_t* req;
  // Response
  resp_t* resp;
  // Buffer for the connection
  buf_t* buf;
} conn_t;

// Create a new connection.
// Do not actually establish the connection.
// But just allocate resource for it.
conn_t* cn_new(int fd, int idx);
// Free a connection
void cn_free(conn_t* conn);

/* pool_t */
typedef struct {
  // list of connections
  size_t n_conns;
  conn_t** conns;

  // info about the pool
  int max_fd;
  size_t n_ready;
  fd_set read_set;
  fd_set write_set;
  fd_set read_ready;
  fd_set write_ready;
} pool_t;

// Create a new pool.
pool_t* pl_new(int sock);
// Free a pool.
void pl_free(pool_t* p);
// Prepare the pool for select.
void pl_ready(pool_t* p);
// Add a connection to pool.
int pl_add_conn(pool_t* p, conn_t* c);
// Delete and free the connection from the pool.
int pl_del_conn(pool_t* p, conn_t* c);

#endif // POOL_H
