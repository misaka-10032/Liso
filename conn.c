/**
 * @file conn.c
 * @brief Implementation of conn.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <sys/socket.h>
#include <openssl/err.h>
#include "conn.h"
#include "logging.h"
#include "utils.h"
#include "config.h"

conn_t* cn_new(int fd, int idx) {
  conn_t* conn = malloc(sizeof(conn_t));
  conn->fd = fd;
  conn->idx = idx;
  conn->req = req_new();
  conn->resp = resp_new();
  conn->cgi = cgi_new();
  conn->buf = buf_new(BUFSZ);
  // ssl status won't change once established
  conn->ssl = NULL;
  conn->ssl_accepted = false;
  return conn;
}

void cn_free(conn_t* conn) {
  if (conn->ssl) {
    SSL_shutdown(conn->ssl);
    SSL_free(conn->ssl);
  }
  req_free(conn->req);
  resp_free(conn->resp);
  cgi_free(conn->cgi);
  buf_free(conn->buf);
  free(conn);
}

int cn_init_ssl(conn_t* conn, SSL_CTX* ctx) {

  if (!(conn->ssl = SSL_new(ctx))) {
    log_errln("[cn_init_ssl] failed to SSL_new for %d.", conn->fd);
    return -1;
  }

  int rc;
  if ((rc = !SSL_set_fd(conn->ssl, conn->fd))) {
    log_errln("[cn_init_ssl] failed to SSL_set_fd for %d with rc %d. %s",
              conn->fd, rc,
              ERR_error_string(SSL_get_error(conn->ssl, rc), NULL));
    SSL_free(conn->ssl);
    conn->ssl = NULL;
    return -1;
  }

  conn->req->scheme = HTTPS;

#if DEBUG >= 1
  log_line("[cn_init_ssl] established ssl with %d.", conn->fd);
#endif
  return 1;
}

static ssize_t smart_recv(SSL* ssl, int fd, void* data, size_t len) {
  if (ssl)
    return SSL_read(ssl, data, len);
  else
    return recv(fd, data, len, 0);
}

static ssize_t smart_send(SSL* ssl, int fd, void* data, size_t len) {
  if (ssl)
    return SSL_write(ssl, data, len);
  else
    return send(fd, data, len, 0);
}

/**
 * @brief Recv and ignore because of previous error
 * @param fat_cb Fatailty callback
 * @return 1 if conn is alive
 *        -1 if conn is closed
 *
 * It will keep recv'ng until client_sock is write ready,
 * and we have sent the error response.
 */
static int recv_ignore(conn_t* conn, FatCb fat_cb) {
  // recv the content anyway
  ssize_t rc = smart_recv(conn->ssl, conn->fd, conn->buf->data, BUFSZ);
  // finally client gives up
  if (rc <= 0)
    //  < 0   =>   fatal, drop
    // == 0   =>   client closed, drop
    return fat_cb(conn);
  else
    return 1;
}

int cn_parse_req(conn_t* conn, void* last_recv_end, ErrCb err_cb) {

  if (conn->req->phase == REQ_START ||
      conn->req->phase == REQ_HEADER) {

    void* last_parsed_end = conn->buf->data_p;

    // Here we need to parse line by line.
    // conn->buf->data_p maintains up to which line we have parsed.
    // We need to copy as much line as possible to the local buffer.
    // After copying, remain data_p at the next position of last \n.
    char *p, *q = conn->buf->data_p - 1;
    for (p = (char*) last_recv_end; p < (char*) buf_end(conn->buf); p++)
      if (*p == '\n')
        q = p + 1;

    // There is something to parse
    if (q > (char*) conn->buf->data_p) {

      buf_t* buf_lines = buf_new();

      buf_lines->sz = q - (char*) conn->buf->data_p;
      memcpy(buf_lines->data, conn->buf->data_p, buf_lines->sz);
      conn->buf->data_p = q;

      ssize_t rc;
      rc = req_parse(conn->req, buf_lines);

      buf_free(buf_lines);

      // handle bad header
      if (rc < 0)
        return err_cb(conn, -rc);
      else
        // point data_p to the end of parsed buffer
        conn->buf->data_p = last_parsed_end + rc;
    }
  }

  if (conn->req->phase == REQ_BODY) {

    // prepare for the transitions
    if (conn->req->type == REQ_STATIC) {

      // it's static request, disable cgi.
      conn->cgi->phase = CGI_DISABLED;

    } else {

      // it's dynamic request, disable resp.
      conn->resp->phase = RESP_DISABLED;

      if (conn->cgi->phase == CGI_IDLE)
        conn->cgi->phase = CGI_READY;
    }

    // effective data is [data_p, data+sz)
    ssize_t size = buf_end(conn->buf) - conn->buf->data_p;
    conn->req->rsize -= size;
    if (conn->req->rsize == 0) {
      conn->req->phase = REQ_DONE;
    } else if (conn->req->rsize < 0) {
      conn->req->phase = REQ_DONE;
      ssize_t rest_sz = -conn->req->rsize;

      buf_reset(conn->req->last_buf);
      conn->req->last_buf->sz = rest_sz;
      memcpy(conn->req->last_buf->data, buf_end(conn->buf)-rest_sz, rest_sz);

      // adjust the right size
      conn->buf->sz -= rest_sz;
    }

    // don't care about body if it's static
    if (conn->req->type == REQ_STATIC)
      buf_reset(conn->buf);
  }

#if DEBUG >= 2
  if (conn->req->phase == REQ_DONE) {
    buf_t* dump = buf_new();
    req_pack(conn->req, dump);
    log_line("[cn_recv] parsed req\n%s", dump->data);
    buf_free(dump);
  }
#endif

  return 1;
}

int cn_recv(conn_t* conn, ErrCb err_cb, FatCb fat_cb) {

  // ssl conn needs SSL_accept first
  if (conn->ssl && !conn->ssl_accepted) {

    int rc = SSL_accept(conn->ssl);

    // error occurs
    if (rc == 0) {
      log_errln("[cn_recv] failed to SSL_accept for %d with rc %d. %s",
                conn->fd, rc,
                ERR_error_string(SSL_get_error(conn->ssl, rc), NULL));
      SSL_free(conn->ssl);
      conn->ssl = NULL;
      return fat_cb(conn);

    // client would block; continue in next iter.
    } else if (rc < 0) {
      return 1;

    // success
    } else {
      conn->ssl_accepted = true;
      return 1;
    }
  }

  // ignore the aborted ones
  // err handling has been done before, so here we just ignore new content.
  if (conn->req->phase == REQ_ABORT)
    return recv_ignore(conn, fat_cb);

  /******** recv msg ********/

  ssize_t rsize = BUFSZ - conn->buf->sz;

  // header to large
  if (rsize <= 0) {
    err_cb(conn, 400);
    return recv_ignore(conn, fat_cb);
  }

  // append to conn buf
  void* last_recv_end = buf_end(conn->buf);
  ssize_t dsize = smart_recv(conn->ssl, conn->fd, last_recv_end, rsize);
  if (dsize < 0) {
    conn->req->phase = REQ_ABORT;
    return fat_cb(conn);
  }

  // update size
  conn->buf->sz += dsize;

  // client closed
  if (dsize == 0) {
#if DEBUG >= 1
    log_line("[cn_recv] client closed from %d", conn->fd);
#endif
    conn->req->phase = REQ_ABORT;
    return fat_cb(conn);
  }

  /******** parse content ********/

#if DEBUG >= 1
  log_line("[cn_recv] parse req for %d", conn->fd);
  log_line("[cn_recv] phase is %d", conn->req->phase);
#endif

  // TODO ok to start with last_recv_end?
  cn_parse_req(conn, last_recv_end, err_cb);

  return 1;
}

int cn_prepare_static_header(conn_t* conn, const conf_t* conf, ErrCb err_cb) {
#if DEBUG >= 1
  log_line("[prepare_static] %d", conn->fd);
#endif

  /* Try to build response */
  if (!resp_build(conn->resp, conn->req, conf))
    return err_cb(conn, conn->resp->status);

  /* prepare header */
  conn->resp->phase = RESP_HEADER;
  buf_reset(conn->buf);
  conn->buf->sz = resp_hdr(conn->resp, conn->buf->data);

#if DEBUG >= 1
  log_line("[prepare_static] Serving static page for %d.", conn->fd);
#endif

#if DEBUG >= 2
  *(char*) buf_end(conn->buf) = 0;
  log_line("[prepare_static] response header for %d is\n%s",
           conn->fd, conn->buf->data);
#endif

  return 1;
}

/**
 * @brief Send error page to client.
 * @param conn Connection.
 * @param succ_cb Success callback.
 * @param fat_cb Fatality callback.
 * @return 1 if conn is alive.
 *        -1 if conn is closed.
 *
 * Assumes error page fits into buffer.
 */
static int cn_send_error_page(conn_t* conn, SuccCb succ_cb, FatCb fat_cb) {

  buf_t* buf = conn->buf;
  req_t* req = conn->req;
  resp_t* resp = conn->resp;

  /**** prepare ****/

  // reset buf so as to put error header/body in it
  buf_reset(buf);

  // sync Connection field
  resp->alive = req->alive;

  // prepare page content
  const char* msg = resp_msg(resp->status);
  // update Content-Length field
  resp->clen = strlen(msg);

  // fill in header
  buf->sz = resp_hdr(resp, (char*) buf->data);

  // fill in body
  memcpy(buf_end(buf), msg, resp->clen);
  buf->sz += resp->clen;

  /**** send ****/

  ssize_t rsize = buf_end(buf) - buf->data_p;
  ssize_t rc = smart_send(conn->ssl, conn->fd, buf->data_p, rsize);
  if (rc <= 0) {
    log_errln("Error sending error msg.");
    return fat_cb(conn);
  }

  buf->data_p += rc;
  if (buf->data_p >= buf_end(buf))
    succ_cb(conn);

  return 1;
}

int cn_serve_static(conn_t* conn, SuccCb succ_cb, FatCb fat_cb) {

  if (conn->resp->phase == RESP_ABORT)
    return cn_send_error_page(conn, succ_cb, fat_cb);

  if (conn->resp->phase == RESP_HEADER) {
    buf_t* buf = conn->buf;
    ssize_t rsize = buf_end(buf) - buf->data_p;

    ssize_t rc = smart_send(conn->ssl, conn->fd, buf->data_p, rsize);

    if (rc <= 0) {
#if DEBUG >= 1
      log_line("[cn_serve_static] Error when sending to %d.", conn->fd);
#endif
      return fat_cb(conn);
    }

#if DEBUG >= 2
    log_line("[cn_serve_static] Sent %zd bytes of header to %d", rc, conn->fd);
#endif

    buf->data_p += rc;
    if (rc == rsize)
      conn->resp->phase = RESP_BODY;
    if (conn->req->method == M_HEAD ||
        conn->resp->mmbuf->data_p >= buf_end(conn->resp->mmbuf))
      return succ_cb(conn);

    // only do one send at a time, so return early.
    return 1;
  }

  if (conn->resp->phase == RESP_BODY) {
    buf_t* buf = conn->resp->mmbuf;
    ssize_t rsize = buf_end(buf) - buf->data_p;
    ssize_t asize = min(BUFSZ, rsize);

    ssize_t rc = smart_send(conn->ssl, conn->fd, buf->data_p, asize);

    if (rc <= 0) {
#if DEBUG >= 1
      if (rc < 0)
        log_line("[cn_serve_static] Error when sending to %d.", conn->fd);
      else
        log_line("[cn_serve_static] Finished sending to %d.", conn->fd);
#endif
      return fat_cb(conn);
    }

#if DEBUG >= 2
    log_line("[cn_serve_static] Sent %zd bytes of body to %d", rc, conn->fd);
#endif

    buf->data_p += rc;
    if (buf->data_p >= buf_end(buf))
      succ_cb(conn);
  }

  return 1;
}

int cn_init_cgi(conn_t* conn, const conf_t* conf,
                SuccCb succ_cb, ErrCb err_cb) {
  if (cgi_init(conn->cgi, conn->req, conf)) {
    conn->cgi->phase = CGI_SRV_TO_CGI;
    return succ_cb(conn);
  } else {
    conn->cgi->phase = CGI_ABORT;
    conn->resp->phase = RESP_ABORT;
    return err_cb(conn, 500);
  }
}

int cn_stream_to_cgi(conn_t* conn, ErrCb err_cb) {

  ssize_t rsize = buf_end(conn->buf) - conn->buf->data_p;

  // we trust cgi's not blocking
  // TODO: do we?
  while (rsize > 0) {
    ssize_t sz = write(conn->cgi->srv_out, conn->buf->data_p, rsize);
    if (sz <= 0)
      return err_cb(conn, 500);

    rsize -= sz;

#if DEBUG >= 2
    log_line("[stream to cgi]");
    log_raw(conn->buf->data_p, sz);
#endif
  }

  // reset buffer in order to recv
  buf_reset(conn->buf);

  return 1;
}

int cn_stream_from_cgi(conn_t* conn, ErrCb err_cb) {

  conn->buf->sz = read(conn->cgi->srv_in, conn->buf->data, BUFSZ);

  if (conn->buf->sz < 0) {
    conn->cgi->phase = CGI_ABORT;
    return err_cb(conn, 500);
  }

  if (conn->buf->sz == 0)
    conn->cgi->phase = CGI_DONE;

  conn->cgi->buf_phase = BUF_SEND;

  return 1;
}

int cn_serve_dynamic(conn_t* conn, SuccCb succ_cb, FatCb fat_cb) {

  buf_t* buf = conn->buf;
  ssize_t rsize = buf_end(buf) - buf->data_p;

  if (rsize == 0 && conn->cgi->phase == CGI_DONE)
    return succ_cb(conn);

  ssize_t rc = smart_send(conn->ssl, conn->fd, buf->data_p, rsize);
  if (rc <= 0) {
    return fat_cb(conn);
  }

  rsize -= rc;
  if (rsize == 0)
    conn->cgi->buf_phase = BUF_RECV;

  return 1;
}
