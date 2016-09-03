/******************************************************************************
* pool.c                                                                      *
*                                                                             *
* Description: Implementation of pool                                         *
*                                                                             *
* Author(s):   Longqi Cai (longqic@andrew.cmu.edu)                            *
*                                                                             *
*******************************************************************************/

#include "pool.h"
#include "utils.h"

conn_t* cn_new(int fd, int idx) {
  conn_t* conn = malloc(sizeof(conn_t));
  conn->fd = fd;
  conn->idx = idx;
  conn->buf = bf_new();
  return conn;
}

void cn_free(conn_t* conn) {
  free(conn->buf);
  free(conn);
}


pool_t* pl_new(int sock) {
  pool_t* p = malloc(sizeof(pool_t));
  p->n_conns = 0;
  p->conns = malloc(sizeof(conn_t) * MAX_EVTS);

  p->max_fd = sock;
  FD_ZERO(&p->read_set);
  FD_ZERO(&p->write_set);
  FD_ZERO(&p->read_ready);
  FD_ZERO(&p->write_ready);
  FD_SET(sock, &p->read_set);
  return p;
}

void pl_ready(pool_t* p) {
  p->read_ready = p->read_set;
  p->write_ready = p->write_set;
}

void pl_free(pool_t* p) {
  free(p->conns);
  free(p);
}

void pl_add_conn(pool_t* p, conn_t* c) {
  // TODO: more than n_conns
  FD_SET(c->fd, &p->read_set);
  c->idx = p->n_conns;
  p->conns[p->n_conns++] = c;
#ifdef DEBUG
  printf("[pl_add_conn] fd=%d, poolsz=%zu.\n", c->fd, p->n_conns);
#endif
}

void pl_del_conn(pool_t* p, conn_t* c) {
  // TODO: non exist
  FD_CLR(c->fd, &p->read_set);
  p->conns[c->idx] = p->conns[--p->n_conns];
  p->conns[p->n_conns] = NULL;
  cn_free(c);
#ifdef DEBUG
  printf("[pl_del_conn] fd=%d, poolsz=%zu.\n", c->fd, p->n_conns);
#endif
}
