/**
 * @file conn.h
 * @brief Handles connections.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 *
 * It coordinates between client, server, and cgi, maintaining buffer
 * as communication channel between them. A connection can either be
 * ssl conn or non-ssl conn, and won't change until it's destroyed.
 */

#ifndef CONN_H
#define CONN_H

#include <openssl/ssl.h>
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
  // Buffer for the connection
  buf_t* buf;
  // Parsed request header
  req_t* req;
  // Response
  resp_t* resp;
  // CGI
  cgi_t* cgi;
  // ssl connection
  SSL* ssl;
  // ssl accept status
  bool ssl_accepted;
} conn_t;

/**
 * @brief Success Callback
 * @param conn Connection
 * @return 1 if conn is still alive
 *        -1 if conn is closed
 */
typedef int (*SuccCb)(conn_t* conn);

/**
 * @brief Error Callback
 * @param conn Connection
 * @param status Status Code
 * @return 1 always
 */
typedef int (*ErrCb)(conn_t* conn, int status);

/**
 * @brief Fatality Callback
 * @param conn Connection
 * @return -1 always
 */
typedef int (*FatCb)(conn_t* conn);

/**
 * @brief Create a new connection
 * @param fd File descriptor of the client socket
 * @param idx Index in the pool
 * @return New connection
 */
conn_t* cn_new(int fd, int idx);

/**
 * @brief Free a connection
 * @param conn Connection to be freed
 */
void cn_free(conn_t* conn);

/**
 * @brief Init an ssl connection.
 * @param conn Connection.
 * @param ctx SSL context.
 * @return 1 if normal.
 *        -1 if error occurs.
 */
int cn_init_ssl(conn_t* conn, SSL_CTX* ctx);

/**
 * @brief Recv content from client.
 * @param conn Connection.
 * @param err_cb Error callback.
 * @param fat_cb Fatality callback.
 * @return 1 if conn is alive.
 *        -1 if conn is closed.
 */
int cn_recv(conn_t* conn, ErrCb err_cb, FatCb fat_cb);

/**
 * @brief Parse req in the buffer.
 * @param conn Connection.
 * @param buf_start Last end of recv in the buffer.
 * @param err_cb Error callback.
 */
int cn_parse_req(conn_t* conn, void* last_recv_end, ErrCb err_cb);

/**
 * @brief Prepare static header in buffer.
 * @param conn Connection.
 * @param conf Global configurations.
 * @param err_cb Error callback.
 * @return 1 always.
 */
int cn_prepare_static_header(conn_t* conn, const conf_t* conf, ErrCb err_cb);

/**
 * @brief Serve static page to client.
 * @param conn Connection.
 * @param succ_cb Success callback.
 * @param fat_cb Fatality callback.
 * @return 1 if conn is alive.
 *        -1 if conn is closed.
 */
int cn_serve_static(conn_t* conn, SuccCb succ_cb, FatCb fat_cb);

/**
 * @brief Init CGI.
 * @param conn Connection.
 * @param conf Global configurations.
 * @param succ_cb Success callback.
 * @param err_cb Error callback.
 * @return 1 always.
 */
int cn_init_cgi(conn_t* conn, const conf_t* conf,
                SuccCb succ_cb, ErrCb err_cb);

/**
 * @brief Stream body to CGI
 * @param conn Connection
 * @param err_cb Error callback
 * @return 1 always
 */
int cn_stream_to_cgi(conn_t* conn, ErrCb err_cb);

/**
 * @brief Stream response from CGI
 * @param conn Connection
 * @param err_cb Error callback
 * @return 1 always
 */
int cn_stream_from_cgi(conn_t* conn, ErrCb err_cb);

/**
 * @brief Serve dynamic content to client
 * @param conn Connection
 * @param succ_cb Success callback
 * @param fat_cb Fatality callback
 * @return 1 if conn is alive
 *        -1 if conn is closed
 *
 * Assuems buf_phase to be SEND.
 * Toggles buf_phase to be RECV if finished sending.
 * Reset or close if CGI also finishes.
 */
int cn_serve_dynamic(conn_t* conn, SuccCb succ_cb, FatCb fat_cb);

#endif // CONN_H
