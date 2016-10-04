/**
 * @file conn.c
 * @brief Implementation of conn.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <sys/socket.h>
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
  return conn;
}

void cn_free(conn_t* conn) {
  req_free(conn->req);
  resp_free(conn->resp);
  cgi_free(conn->cgi);
  buf_free(conn->buf);
  free(conn);
}

/**
 * @brief Recv and ignore because of previous error
 * @param fat_cb Fatailty callback
 * @return 1 if conn is alive
 *        -1 if conn is closed
 */
static int recv_ignore(conn_t* conn, FatCb fat_cb) {
  // recv the rest anyway
  ssize_t rc = recv(conn->fd, conn->buf->data, BUFSZ, 0);
  // finally you stopped talking
  if (rc <= 0) {
    return fat_cb(conn);
  } else {
    return 1;
  }
}

int cn_recv(conn_t* conn, ErrCb err_cb, FatCb fat_cb) {

  // ignore the aborted ones
  if (conn->req->phase == REQ_ABORT) {
    return recv_ignore(conn, fat_cb);
  }

  /******** recv msg ********/

  ssize_t rsize = BUFSZ - conn->buf->sz;

  // header to large
  if (rsize <= 0) {
    err_cb(conn, 400);
    return recv_ignore(conn, fat_cb);
  }

  // append to conn buf
  ssize_t dsize = recv(conn->fd, buf_end(conn->buf), rsize, 0);
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

  if (conn->req->phase == REQ_START ||
      conn->req->phase == REQ_HEADER) {

    // Here we need to parse line by line.
    // conn->buf->data_p maintains up to which line we have parsed.
    // We need to copy as much line as possible to the local buffer.
    // After copying, remain data_p at the next position of last \n.

    char *p, *q = conn->buf->data_p - 1;
    for (p = (char*) conn->buf->data_p; p < (char*) buf_end(conn->buf); p++)
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
        err_cb(conn, -rc);
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
    if (conn->req->rsize <= 0) {

      conn->req->phase = REQ_DONE;

      // recv'ed more than required
      conn->buf->sz += conn->req->rsize;
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
  ssize_t rc = send(conn->fd, buf->data_p, rsize, 0);
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

    ssize_t rc = send(conn->fd, buf->data_p, rsize, 0);

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

    ssize_t rc = send(conn->fd, buf->data_p, asize, 0);

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

  ssize_t rc = send(conn->fd, buf->data_p, rsize, 0);
  if (rc <= 0) {
    return fat_cb(conn);
  }

  rsize -= rc;
  if (rsize == 0)
    conn->cgi->buf_phase = BUF_RECV;

  return 1;
}
