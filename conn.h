/**
 * @file conn.h
 * @brief Handles connections
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef CONN_H
#define CONN_H

#include "buffer.h"
#include "request.h"
#include "response.h"
#include "cgi.h"

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
  // CGI
  cgi_t* cgi;
  // Buffer for the connection
  buf_t* buf;
} conn_t;

// Create a new connection.
// Do not actually establish the connection.
// But just allocate resource for it.
conn_t* cn_new(int fd, int idx);
// Free a connection
void cn_free(conn_t* conn);

#endif // CONN_H
