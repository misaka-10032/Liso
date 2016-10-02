/**
 * @file lisod.c
 * @brief Entry for the Liso server.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 *
 * This is a select-based server, running as daemon.
 * It serves static pages, and support cgi scripts.
 *
 */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <sys/stat.h>
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
#include "config.h"
#include "utils.h"

// global listener socket
static int sock;
// global connection pool
static pool_t* pool;

// global arguments
static const int ARG_CNT = 8;
static conf_t conf;

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

// reset req/resp or recycle them.
// always return 1 indicating no mistake.
static int liso_reset_or_recycle(conn_t* conn) {
  if (!conn->resp->alive) {
    return cleanup(conn);
  }

  FD_SET(conn->fd, &pool->read_set);
  FD_CLR(conn->fd, &pool->write_set);

  if (conn->cgi->srv_in > 0)
    FD_CLR(conn->cgi->srv_in, &pool->read_set);

  if (conn->cgi->srv_out > 0)
    FD_CLR(conn->cgi->srv_out, &pool->write_set);

  buf_reset(conn->buf);
  req_reset(conn->req);
  resp_reset(conn->resp);
  cgi_reset(conn->cgi);

  return 1;
}

// tear down the server
static int teardown(int rc) {
  close_socket(sock);
  pl_free(pool);
  if (log_inited()) {
    log_line("-------- Liso Server stops --------");
    log_stop();
  }
  fprintf(stderr, "Lisod terminated with rc %d.\n", rc);
  exit(rc);
  // don't return though
  return rc;
}

static void signal_handler(int sig) {
  switch (sig) {
    case SIGHUP:
      /* rehash the server */
      break;
    case SIGTERM:
      teardown(EXIT_SUCCESS);
      break;
    default:
      break;
  }
}

// daemonize the process
static int daemonize(char* lock_file) {
  /* drop to having init() as parent */
  int i, lfp, pid = fork();
  char str[256] = {0};
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) {
    fflush(stdout);
    fflush(stderr);
    for (i = getdtablesize(); i >= 0; i--)
      close(i);
    exit(EXIT_SUCCESS);
  }

  setsid();

  lfp = open(lock_file, O_RDWR|O_CREAT, 0640);

  if (lfp < 0) {
    fprintf(stderr, "Cannot open lock file.\n");
    exit(EXIT_FAILURE); /* can not open */
  }

  if (lockf(lfp, F_TLOCK, 0) < 0) {
    fprintf(stderr, "Cannot lock the lock file.\n");
    exit(EXIT_FAILURE); /* can not lock */
  }

  /* only first instance continues */
  pid = getpid();
  sprintf(str, "%d\n", pid);
  write(lfp, str, strlen(str)); /* record pid to lockfile */

  signal(SIGCHLD, SIG_IGN); /* child terminate signal */
  signal(SIGHUP, signal_handler); /* hangup signal */
  signal(SIGTERM, signal_handler); /* software termination signal from kill */

  printf("Successfully daemonized lisod, pid %d.\n", pid);

  i = open("/dev/null", O_RDWR);
  dup(i); /* stdout */
  // TODO: dup stderr?
  umask(027);

  return EXIT_SUCCESS;
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

// switch from recv mode to send mode for static pages
// called only when request is totally recv'ed
// returns -1 if conn is cleaned up
//          1 if normal.
static int liso_prepare_static(conn_t* conn) {
#if DEBUG >= 1
  log_line("[prepare_static] %d", conn->fd);
#endif

  FD_CLR(conn->fd, &pool->read_set);
  FD_SET(conn->fd, &pool->write_set);

  /* Try to build response */
  if (!resp_build(conn->resp, conn->req, &conf))
    return prepare_error_resp(conn, conn->resp->status);

  /* prepare header */
  conn->resp->phase = RESP_HEADER;
  buf_reset(conn->buf);
  conn->buf->sz = resp_hdr(conn->resp, conn->buf->data);

#if DEBUG >= 1
  log_line("[recv_to_send] Serving static page for %d.", conn->fd);
#endif

#if DEBUG >= 2
  *(char*) buf_end(conn->buf) = 0;
  log_line("[recv_to_send] response header for %d is\n%s",
           conn->fd, conn->buf->data);
#endif

  return 1;
}

// stream request body to cgi program
// called only when header is totally recv'ed
// body waits to be recv'ed at this point
// returns -1 if conn is cleaned up
//          1 if normal.
static int liso_stream_to_cgi(conn_t* conn) {

  if (conn->cgi->phase == CGI_READY) {

    if (!cgi_init(conn->cgi, conn->req, &conf)) {
      conn->cgi->phase = CGI_ABORT;
      conn->resp->phase = RESP_ABORT;
      conn->resp->status = 500;
      return prepare_error_resp(conn, conn->resp->status);
    }

    FD_SET(conn->cgi->srv_out, &pool->write_set);
    FD_SET(conn->cgi->srv_in, &pool->read_set);
  }

  ssize_t rsize = buf_end(conn->buf) - conn->buf->data_p;
  while (rsize > 0) {
    ssize_t sz = write(conn->cgi->srv_out, conn->buf->data_p, rsize);
    if (sz <= 0)
      return prepare_error_resp(conn, 500);
    rsize -= sz;
  }

  return 1;
}

// stream response from cgi program
// mark DONE if streaming is finished
// returns -1 if conn is cleaned up
//          1 if normal.
static int liso_stream_from_cgi(conn_t* conn) {

  conn->buf->sz = read(conn->cgi->srv_in, conn->buf->data, BUFSZ);

  if (conn->buf->sz < 0) {
    conn->cgi->phase = CGI_ABORT;
    return prepare_error_resp(conn, 500);
  }

  if (conn->buf->sz == 0)
    conn->cgi->phase = CGI_DONE;

  return 1;
}

// serve dynamic content to conn
// assumes buf_phase of SEND
// toggle buf_phase to be RECV if finished sending
// reset or recycle if CGI also finishes
// returns -1 if conn is cleaned up.
//          1 if normal.
static int liso_serve_dynamic(conn_t* conn) {
  buf_t* buf = conn->buf;
  ssize_t rsize = buf_end(buf) - buf->data_p;
  ssize_t rc = send(conn->fd, buf->data_p, rsize, 0);

  if (rc <= 0) {
    return cleanup(conn);
  }

  rsize -= rc;
  if (rsize == 0) {
    if (conn->cgi->buf_phase == BUF_DONE)
      return liso_reset_or_recycle(conn);
    else
      conn->cgi->buf_phase = BUF_RECV;
  }

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
//          1 if normal.
static int liso_recv(conn_t* conn) {

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
    if (conn->req->type == REQ_STATIC)
      conn->cgi->phase = CGI_DISABLED;
    else
      conn->resp->phase = RESP_DISABLED;

    // effective data is [data_p, data+sz)
    ssize_t size = buf_end(conn->buf) - conn->buf->data_p;
    conn->req->rsize -= size;
    if (conn->req->rsize <= 0) {
      conn->req->phase = REQ_DONE;
    }
    // after streaming body, reset data pointer
    buf_reset(conn->buf);
  }

#if DEBUG >= 2
  if (conn->req->phase == REQ_DONE) {
    buf_t* dump = buf_new();
    req_pack(conn->req, dump);
    log_line("[main loop] parsed req\n%s", dump->data);
    buf_free(dump);
  }
#endif

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

// serve static content to conn
// returns -1 if conn is cleaned up.
//         1  if normal.
static int liso_serve_static(conn_t* conn) {

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
      if (rc < 0)
        log_line("[liso_send] Error when sending to %d.", conn->fd);
      else
        log_line("[liso_send] Finished sending to %d.", conn->fd);
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

  conf.http_port = atoi(argv[1]);
  conf.https_port = atoi(argv[2]);
  conf.log = argv[3];
  conf.lock = argv[4];
  conf.www = argv[5];
  conf.cgi = argv[6];
  conf.prv = argv[7];
  conf.crt = argv[8];

  /* create listener socket */
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Failed creating listener socket. "
                    "Server not started.\n");
    log_errln("Failed creating socket.");
    teardown(EXIT_FAILURE);
  }

  /* init conn pool */
  pool = pl_new(sock);

  /* bind port */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(conf.http_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
    fprintf(stderr, "Failed binding the port. "
                    "Server not started.\n");
    teardown(EXIT_FAILURE);
  }

  if (listen(sock, 5)) {
    fprintf(stderr, "Error listening on socket. "
                    "Server not started.\n");
    teardown(EXIT_FAILURE);
  }

  // avoid crash when client continues to send after sock is closed.
  signal(SIGPIPE, SIG_IGN);
  // daemonize server
  daemonize(conf.lock);

  /* setup log */
  log_init(conf.log);
  log_line("-------- Liso Server starts --------");

  /******** main loop ********/
  while (1) {

    // get pool ready
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

      conn_t* conn = pool->conns[i];

      /* recv */
      if (FD_ISSET(conn->fd, &pool->read_ready)) {
        if (liso_recv(conn) < 0) {
          // the fatal conn is cleaned up and the last one replaces it.
          // go back and forward to process the new connection.
          i -= 1;
          continue;
        }
      }

      /* transitions */

      if (conn->req->phase == REQ_DONE &&
          conn->resp->phase == RESP_READY) {
        // only prepare once
        liso_prepare_static(conn);
      }

      if (conn->req->phase == REQ_BODY &&
          conn->req->type == REQ_DYNAMIC) {
        // assumes we can stream conn->buf->data_p
        // to pipe very fast
        liso_stream_to_cgi(conn);
      }

      if (conn->req->phase == REQ_DONE &&
          conn->req->type == REQ_DYNAMIC) {
        // cgi stream out/in transition
        liso_stream_to_cgi(conn);
        close_pipe(&conn->cgi->srv_out);
        conn->cgi->phase = CGI_CGI_TO_SRV;
        FD_SET(conn->cgi->srv_in, &pool->read_set);
      }

      /* serve */

      if (conn->req->type == REQ_DYNAMIC &&
          conn->cgi->phase == CGI_CGI_TO_SRV) {

        if (FD_ISSET(conn->cgi->srv_in, &pool->read_ready) &&
            conn->cgi->buf_phase == BUF_RECV) {
          liso_stream_from_cgi(conn);
        }

        if (FD_ISSET(conn->fd, &pool->write_ready) &&
            conn->cgi->buf_phase == BUF_SEND) {
          if (liso_serve_dynamic(conn) < 0) {
            i -= 1;
            continue;
          }
        }
      }

      if (FD_ISSET(conn->fd, &pool->write_ready) &&
          conn->resp->phase != RESP_DISABLED) {
        // serves several cases
        // 1. serving static request
        // 2. bad request
        // 3. cgi error
        if (liso_serve_static(conn) < 0) {
          i -= 1;
          continue;
        }
      }

      /* maintain max_fd */
      max_fd = max(max_fd, conn->fd);
      max_fd = max(max_fd, conn->cgi->srv_in);
      max_fd = max(max_fd, conn->cgi->srv_out);
    }

    // update max_fd
    pool->max_fd = min(pool->max_fd, max_fd);
#if DEBUG >= 1
    log_line("[main loop] max fd is %d", pool->max_fd);
#endif
  }

  return teardown(EXIT_SUCCESS);
}
