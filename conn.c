/**
 * @file conn.c
 * @brief Implementation of conn.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include "conn.h"

conn_t* cn_new(int fd, int idx) {
  conn_t* conn = malloc(sizeof(conn_t));
  conn->fd = fd;
  conn->idx = idx;
  conn->req = req_new();
  conn->resp = resp_new();
  conn->cgi = cgi_new();
  conn->buf = buf_new(BUFSZ);
  return conn;
}

void cn_free(conn_t* conn) {
  req_free(conn->req);
  resp_free(conn->resp);
  cgi_free(conn->cgi);
  buf_free(conn->buf);
  free(conn);
}
