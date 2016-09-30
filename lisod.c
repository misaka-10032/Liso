/**
 * @file lisod.c
 * @brief Entry for the Liso server.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 *
 * TODO This is a select-based server.
 *
 */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "pool.h"
#include "request.h"
#include "response.h"
#include "logging.h"
#include "io.h"
#include "utils.h"

// global listener socket
static int sock;
// global connection pool
static pool_t* pool;

// global arguments
static const int ARG_CNT = 8;
static int http_port;
// static int https_port;
static char* log_file;
// static char* lock_file;
static char* www_folder;
// static char* cgi_path;
// static char* prvkey_file;
// static char* cert_file;

// close a socket
int close_socket(int sock) {
  if (close(sock)) {
    log_errln("Failed closing socket %d.", sock);
    return 1;
  }
  return 0;
}

// clean up the connection
// always return -1
static int cleanup(conn_t* conn) {
#if DEBUG >= 1
  log_line("[cleanup] %d.", conn->fd);
#endif
  int yes = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  close_socket(conn->fd);
  if (pl_del_conn(pool, conn) < 0) {
    log_errln("Error deleting connection.");
  }
  return -1;
}

// tear down the server
static void teardown(int rc) {
  close_socket(sock);
  pl_free(pool);
  log_line("-------- Liso Server stops --------");
  log_stop();
  exit(rc);
}

static void signal_handler(int sig) {
  switch (sig) {
    case SIGINT:
    case SIGTERM:
      teardown(EXIT_SUCCESS);
      break;
  }
}

// recv error, prepare to respond error
// always return 1
static int prepare_error_resp(conn_t* conn, int status) {
  conn->req->phase = REQ_ABORT;
  conn->resp->phase = RESP_ABORT;
  conn->resp->status = status;
  FD_SET(conn->fd, &pool->write_set);
  return 1;
}

// switch from recv mode to send mode
// called only when recv succeeds
// returns -1 if conn is cleaned up
//          1 if normal.
static int liso_recv_to_send(conn_t* conn) {
#if DEBUG >= 1
  log_line("[recv_to_send] %d", conn->fd);
#endif

  FD_CLR(conn->fd, &pool->read_set);
  FD_SET(conn->fd, &pool->write_set);

  /* Try to build response */
  if (!resp_build(conn->resp, conn->req, www_folder))
    return prepare_error_resp(conn, conn->resp->status);

  /* prepare header */
  conn->resp->phase = RESP_HEADER;
  buf_reset(conn->buf);
  conn->buf->sz = resp_hdr(conn->resp, conn->buf->data);

#if DEBUG >= 2
  *(char*) buf_end(conn->buf) = 0;
  log_line("[resp_hdr] response header for %d is\n%s",
           conn->fd, conn->buf->data);
#endif

  return 1;
}

// recv and abort
// returns -1 if conn will be cleaned up
//          1 if normal
static int recv_abort(conn_t* conn) {
  // recv the rubbish anyway
  ssize_t rc = recv(conn->fd, conn->buf->data, BUFSZ, 0);
  // finally you stopped talking
  if (rc <= 0) {
    return cleanup(conn);
  } else {
    return 1;
  }
}

// recv from conn
// returns -1 if fatal error occurs, conn cleaned up.
//          0 if not ready, may still needs send.
//          1 if normal.
static int liso_recv(conn_t* conn) {

  if (!FD_ISSET(conn->fd, &pool->read_ready))
    return 0;

  // ignore the aborted ones
  if (conn->req->phase == REQ_ABORT) {
    return recv_abort(conn);
  }

  /******** recv msg ********/
  ssize_t rsize = BUFSZ - conn->buf->sz;

  // header to large
  if (rsize <= 0) {
    prepare_error_resp(conn, 400);
    return recv_abort(conn);
  }

  // append to conn buf
  ssize_t dsize = recv(conn->fd, buf_end(conn->buf), rsize, 0);
  if (dsize < 0) {
    conn->req->phase = REQ_ABORT;
    cleanup(conn);
    log_errln("Error in recv: %s", strerror(errno));
    errno = 0;
    return -1;
  }
  // update size
  conn->buf->sz += dsize;

  /**** client closed ****/
  if (dsize == 0) {
#if DEBUG >= 1
    log_line("[liso_recv] client closed from %d", conn->fd);
#endif
    conn->req->phase = REQ_ABORT;
    return cleanup(conn);
  }

  /******** parse content ********/
#if DEBUG >= 1
  log_line("[liso_recv] parse req for %d", conn->fd);
  log_line("[liso_recv] phase is %d", conn->req->phase);
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
        return prepare_error_resp(conn, -rc);
    }
  }

  if (conn->req->phase == REQ_BODY) {
    // TODO: stream body. effective data is [data_p, data+sz)
    ssize_t size = buf_end(conn->buf) - conn->buf->data_p;
    conn->req->rsize -= size;
    if (conn->req->rsize <= 0) {
      conn->req->phase = REQ_DONE;
    }
    // after streaming body, reset data pointer
    buf_reset(conn->buf);
  }

  if (conn->req->phase == REQ_DONE) {
#if DEBUG >= 2
    buf_t* dump = buf_new();
    req_pack(conn->req, dump);
    log_line("[main loop] parsed req\n%s", dump->data);
    buf_free(dump);
#endif
    // change mode
    return liso_recv_to_send(conn);
  }

  return 1;
}

// reset req/resp or recycle them.
// always return 1 indicating no mistake.
static int liso_reset_or_recycle(conn_t* conn) {
  if (!conn->resp->alive) {
    return cleanup(conn);
  }

  buf_reset(conn->buf);
  req_reset(conn->req);
  resp_reset(conn->resp);
  FD_SET(conn->fd, &pool->read_set);
  FD_CLR(conn->fd, &pool->write_set);

  return 1;
}

// prepare the error msg
static void liso_prepare_error_page(conn_t* conn) {
  buf_t* buf = conn->buf;
  req_t* req = conn->req;
  resp_t* resp = conn->resp;

  // reset buf so as to put error header/body in it
  buf_reset(buf);

  // sync Connection field
  resp->alive = req->alive;

  // update Content-Length field
  const char* msg = resp_msg(resp->status);
  resp->clen = strlen(msg);

  buf->sz = resp_hdr(resp, (char*) buf->data);
  memcpy(buf_end(buf), msg, resp->clen);
  buf->sz += resp->clen;
  conn->resp->phase = RESP_ERROR;
}

// send err msg to client
static int liso_handle_error(conn_t* conn) {
  buf_t* buf = conn->buf;
  ssize_t rsize = buf_end(buf) - buf->data_p;

  ssize_t rc = send(conn->fd, buf->data_p, rsize, 0);

  if (rc <= 0) {
    log_errln("Error sending error msg.");
    return cleanup(conn);
  }

  buf->data_p += rc;
  if (buf->data_p >= buf_end(buf))
    liso_reset_or_recycle(conn);
  return 1;
}

// send to conn
// returns -1 if conn is cleaned up.
//         0  if not ready.
//         1  if normal.
static int liso_send(conn_t* conn) {

  if (!FD_ISSET(conn->fd, &pool->write_ready))
    return 0;

  if (conn->resp->phase == RESP_ABORT)
    liso_prepare_error_page(conn);

  if (conn->resp->phase == RESP_ERROR)
    return liso_handle_error(conn);

  if (conn->resp->phase == RESP_HEADER) {
    buf_t* buf = conn->buf;
    ssize_t rsize = buf_end(buf) - buf->data_p;

    ssize_t rc = send(conn->fd, buf->data_p, rsize, 0);

    if (rc <= 0) {
#if DEBUG >= 1
      log_line("[liso_send] Error when sending to %d.", conn->fd);
#endif
      return cleanup(conn);
    }

#if DEBUG >= 2
    log_line("[liso_send] Sent %zd bytes of header to %d", rc, conn->fd);
#endif

    buf->data_p += rc;
    if (rc == rsize)
      conn->resp->phase = RESP_BODY;
    if (conn->req->method == M_HEAD ||
        conn->resp->mmbuf->data_p >= buf_end(conn->resp->mmbuf))
      return liso_reset_or_recycle(conn);

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
      log_line("[liso_send] Error when sending to %d.", conn->fd);
#endif
      return cleanup(conn);
    }

#if DEBUG >= 2
    log_line("[liso_send] Sent %zd bytes of body to %d", rc, conn->fd);
#endif

    buf->data_p += rc;
    if (buf->data_p >= buf_end(buf))
      liso_reset_or_recycle(conn);
  }

  return 1;
}

int main(int argc, char* argv[]) {

  int client_sock;
  socklen_t cli_size;
  struct sockaddr_in addr, cli_addr;
  int i;

  if (argc != ARG_CNT+1) {
    fprintf(stdout, "Usage: %s <HTTP port> <HTTPS port> <log file>"
                    "<lock file> <www folder> <CGI script path>"
                    "<private key file> <certificate file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  http_port = atoi(argv[1]);
//  https_port = atoi(argv[2]);
  log_file = argv[3];
//  lock_file = argv[4];
  www_folder = argv[5];
//  cgi_path = argv[6];
//  prvkey_file = argv[7];
//  cert_file = argv[8];

  /* setup signals */
  // avoid crash when client continues to send after sock is closed.
  signal(SIGPIPE, SIG_IGN);
  // cgi shouldn't crash lisod.
  signal(SIGCHLD, SIG_IGN);
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  /* setup log */
  log_init(log_file);
  log_line("-------- Liso Server starts --------");

  /* create listener socket */
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    log_errln("Failed creating socket.");
    teardown(EXIT_FAILURE);
  }

  /* init conn pool */
  pool = pl_new(sock);

  /* bind port */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(http_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
    log_errln("Failed binding socket.");
    teardown(EXIT_FAILURE);
  }

  if (listen(sock, 5)) {
    log_errln("Error listening on socket.");
    teardown(EXIT_FAILURE);
  }

  /******** main loop ********/
  while (1) {

    pl_ready(pool);
    /* select those who are ready */
    if ((pool->n_ready = select(pool->max_fd+1,
                                &pool->read_ready,
                                &pool->write_ready,
                                NULL, NULL)) == -1) {
      log_errln("Error in select.");
      continue;
    }
#if DEBUG >= 2
    log_line("[select] n_ready=%zu", pool->n_ready);
#endif

    /**** new connection ****/
    if (FD_ISSET(sock, &pool->read_ready)) {
      cli_size = sizeof(cli_addr);
      if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                &cli_size)) == -1) {
        log_errln("Error in accept: %s", strerror(errno));
        errno = 0;
        continue;
      }

      // Make client_sock non-blocking.
      // It's possible that though server sees it's ready,
      // but the client is then interrupted for something else.
      // We don't want to wait the client indefinitely,
      // so simply return EWOULDBLOCK in that case.
      fcntl(client_sock, F_SETFL, O_NONBLOCK);

      if (pool->n_conns == MAX_CONNS) {
        log_errln("Max conns reached; drop client %d.", client_sock);
        close_socket(client_sock);
        continue;
      }

      pool->max_fd = max(pool->max_fd, client_sock);
      conn_t* conn = cn_new(client_sock, pool->n_conns);

      if (pl_add_conn(pool, conn) < 0) {
        log_errln("Error adding connection.");
        continue;
      }
    }

    /**** serve connections ****/

    int max_fd = sock;

    for (i = 0; i < pool->n_conns; i++) {

      max_fd = max(max_fd, pool->conns[i]->fd);

      int rc;
      rc = liso_recv(pool->conns[i]);
      if (rc < 0) {
        // the fatal conn is cleaned up and the last one replaces it.
        // go back and forward to process the new connection
        i -= 1;
        continue;
      }

      rc = liso_send(pool->conns[i]);
    }

    // update max_fd
    pool->max_fd = min(pool->max_fd, max_fd);
#if DEBUG >= 1
    log_line("[main loop] max fd is %d", pool->max_fd);
#endif
  }

  teardown(EXIT_SUCCESS);
}
