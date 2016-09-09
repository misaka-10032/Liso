/**
 * @file pool.c
 * @brief Implementation of pool.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include "pool.h"
#include "io.h"
#include "logging.h"
#include "utils.h"

conn_t* cn_new(int fd, int idx) {
  conn_t* conn = malloc(sizeof(conn_t));
  conn->fd = fd;
  conn->idx = idx;
  conn->remained = BUFSZ;
  conn->req = req_new();
  conn->buf = buf_new();
  return conn;
}

void cn_free(conn_t* conn) {
  req_free(conn->req);
  buf_free(conn->buf);
  free(conn);
}

pool_t* pl_new(int sock) {
  pool_t* p = malloc(sizeof(pool_t));
  p->n_conns = 0;
  p->conns = malloc(sizeof(conn_t) * MAX_CONNS);

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
  int i;
  for (i = 0; i < p->n_conns; i++)
    cn_free(p->conns[i]);
  free(p->conns);
  free(p);
}

int pl_add_conn(pool_t* p, conn_t* c) {
  if (p->n_conns >= MAX_CONNS)
    return -1;
  FD_SET(c->fd, &p->read_set);
  c->idx = p->n_conns;
  p->conns[p->n_conns++] = c;
#if DEBUG >= 1
  log_line("[pl_add_conn] fd=%d, poolsz=%zu.", c->fd, p->n_conns);
#endif
  return 1;
}

int pl_del_conn(pool_t* p, conn_t* c) {
  if (c->idx >= p->n_conns) {
#if DEBUG >= 1
    log_line("[pl_del_conn] c->idx=%d, p->n_conns=%zu.",
             c->idx, p->n_conns);
#endif
    return -1;
  }
  FD_CLR(c->fd, &p->read_set);
  p->conns[c->idx] = p->conns[--p->n_conns];
  p->conns[c->idx]->idx = c->idx;
  p->conns[p->n_conns] = NULL;
#if DEBUG >= 1
  log_line("[pl_del_conn] fd=%d, idx=%d, poolsz=%zu.",
           c->fd, c->idx, p->n_conns);
#endif
  cn_free(c);
  return 1;
}
