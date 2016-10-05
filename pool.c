/**
 * @file pool.c
 * @brief Implementation of pool.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include "pool.h"
#include "logging.h"
#include "utils.h"

pool_t* pl_new(int sock, int ssl_sock) {
  pool_t* p = malloc(sizeof(pool_t));
  p->n_conns = 0;
  p->conns = malloc(sizeof(conn_t*) * (MAX_CONNS+1));
  p->min_max_fd = max(sock, ssl_sock);
  p->max_fd = p->min_max_fd;

  FD_ZERO(&p->read_set);
  FD_ZERO(&p->write_set);
  FD_ZERO(&p->read_ready);
  FD_ZERO(&p->write_ready);

  FD_SET(sock, &p->read_set);
  FD_SET(ssl_sock, &p->read_set);

  return p;
}

void pl_ready(pool_t* p) {
  p->read_ready = p->read_set;
  p->write_ready = p->write_set;
}

void pl_free(pool_t* p) {
  if (!p)
    return;

  int i;
  for (i = 0; i < p->n_conns; i++)
    cn_free(p->conns[i]);
  free(p->conns);

  free(p);
}

int pl_add_conn(pool_t* p, conn_t* c) {

  if (p->n_conns >= MAX_CONNS) {
    log_errln("[pl_add_conn] poolsz=%zu, totsz=%zu.",
              p->n_conns, MAX_CONNS);
    return -1;
  }

  FD_SET(c->fd, &p->read_set);

  c->idx = p->n_conns;
  p->conns[p->n_conns++] = c;

#if DEBUG >= 1
  log_line("[pl_add_conn] fd=%d, poolsz=%zu, totsz=%zu.",
           c->fd, p->n_conns, MAX_CONNS);
#endif

  return 1;
}

int pl_del_conn(pool_t* p, conn_t* c) {

  if (c->idx >= p->n_conns) {
    log_errln("[pl_del_conn] c->idx=%d, p->n_conns=%zu.",
              c->idx, p->n_conns);
    return -1;
  }

  // order matters; we want to clear all.
  pl_reset_conn(p, c);
  FD_CLR(c->fd, &p->read_set);
  close(c->fd);

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

int pl_reset_conn(pool_t* pool, conn_t* conn) {

  FD_SET(conn->fd, &pool->read_set);
  FD_CLR(conn->fd, &pool->write_set);

  if (conn->cgi->srv_in >= 0) {
    FD_CLR(conn->cgi->srv_in, &pool->read_set);
    close_pipe(&conn->cgi->srv_in);
  }

  if (conn->cgi->srv_err >= 0) {
    FD_CLR(conn->cgi->srv_err, &pool->read_set);
    close_pipe(&conn->cgi->srv_err);
  }

  buf_reset(conn->buf);
  req_reset(conn->req);
  resp_reset(conn->resp);
  cgi_reset(conn->cgi);

  return 1;
}
