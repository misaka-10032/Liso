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
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include "daemon.h"
#include "pool.h"
#include "logging.h"
#include "config.h"
#include "utils.h"

// listener socket
static int sock = -1;

// ssl sock and context
static int ssl_sock = -1;
static SSL_CTX* ssl_ctx = NULL;

// connection pool
static pool_t* pool = NULL;

// input arguments
static const int ARG_CNT = 8;
static conf_t conf;

// tear down the server with rc as return code
static int teardown(int rc) {

  // allow port reuse
  int yes = 1;
  if (sock > 0)
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  if (ssl_sock > 0)
    setsockopt(ssl_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

  if (sock > 0)
    close(sock);

  if (ssl_sock > 0)
    close(ssl_sock);

  release_lock();

  if (ssl_ctx)
    SSL_CTX_free(ssl_ctx);

  if (pool)
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

// global signal handler
static void signal_handler(int sig) {
  int status;
  int pid;
  switch (sig) {
    case SIGCHLD:
      /* reap child to prevent zombie */
      while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0);
      break;
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

// create an ssl context
// takes file path to private key and certificate file
// exit on error
// returns
static SSL_CTX* new_ssl_ctx(const char* prv, const char* crt) {

  SSL_CTX* ssl_ctx;
  SSL_load_error_strings();
  SSL_library_init();

  if (!(ssl_ctx = SSL_CTX_new(TLSv1_server_method()))) {
    fprintf(stderr, "[new_ssl_ctx] Error creating SSL context. "
                    "Server NOT started.\n");
    teardown(EXIT_FAILURE);
  }

  if (!SSL_CTX_use_PrivateKey_file(ssl_ctx, prv, SSL_FILETYPE_PEM)) {
    fprintf(stderr, "[new_ssl_ctx] Error associating private key. "
                    "Server NOT started.\n");
    teardown(EXIT_FAILURE);
  }

  if (!SSL_CTX_use_certificate_file(ssl_ctx, crt, SSL_FILETYPE_PEM)) {
    fprintf(stderr, "[new_ssl_ctx] Error associating certificate. "
                    "Server NOT started.\n");
    teardown(EXIT_FAILURE);
  }

  if (!SSL_CTX_check_private_key(ssl_ctx)) {
    fprintf(stderr, "[new_ssl_ctx] Error checking private key. "
                    "Server NOT started.\n");
  }

  return ssl_ctx;
}

// open a listener socket on port and listen on it
// return the listener socket
// exit on error
static int open_listener_socket(int port) {

  // create listener socket
  int sock;
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Failed creating listener socket. "
                    "Server not started.\n");
    log_errln("Failed creating socket.");
    teardown(EXIT_FAILURE);
  }

  // bind port
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
    fprintf(stderr, "Failed binding the port %d. "
                    "Server not started.\n", port);
    teardown(EXIT_FAILURE);
  }

  // listen on sock
  if (listen(sock, MAX_CONNS)) {
    fprintf(stderr, "Error listening on socket. "
                    "Server not started.\n");
    teardown(EXIT_FAILURE);
  }

  return sock;
}

// accept and establish a conn from sock.
// take it as ssl conn if ctx is passed in.
// if success, conn will be added to pool.
// return the newly established conn.
//        NULL if err occurs.
static conn_t* liso_accept_conn(int sock, SSL_CTX* ctx) {

  int client_sock;
  struct sockaddr_in cli_addr;
  socklen_t cli_size = sizeof(cli_addr);

  if ((client_sock = accept(sock, (struct sockaddr*) &cli_addr,
                            &cli_size)) < 0) {
    log_errln("Error in accept sock: %s", strerror(errno));
    errno = 0;
    return NULL;
  }

  if (client_sock >= FD_SETSIZE) {
    log_errln("client_sock %d getting too large.", client_sock);
    close(client_sock);
    return NULL;
  }

  // Make client_sock non-blocking.
  // It's possible that though server sees it's ready,
  // but the client is then interrupted for something else.
  // We don't want to wait the client indefinitely,
  // so simply return EWOULDBLOCK in that case.
  int flag = fcntl(client_sock, F_GETFL, 0);
  fcntl(client_sock, F_SETFL, flag|O_NONBLOCK);

  if (pool->n_conns == MAX_CONNS) {
    log_errln("Max conns reached; drop client %d.", client_sock);
    close(client_sock);
    return NULL;
  }

  conn_t* conn = cn_new(client_sock, pool->n_conns);

  // remember addr and port
  inet_ntop(AF_INET, &(cli_addr.sin_addr),
            conn->req->addr, INET_ADDRSTRLEN);
  conn->req->port = ctx ? conf.https_port : conf.http_port;

  if (ctx) {
    if (cn_init_ssl(conn, ctx) < 0) {
      if (close(client_sock) < 0) {
        log_errln("[ssl_init] Failed to close client sock.");
      }
      cn_free(conn);
      return NULL;
    }
  }

  if (pl_add_conn(pool, conn) < 0) {
    log_errln("Error in add conn.");
    return NULL;
  }

  return conn;
}

// clean up the connection
// return -1 always
static int liso_drop_conn(conn_t* conn) {
#if DEBUG >= 1
  log_line("[liso_drop_conn] %d.", conn->fd);
#endif

  if (pl_del_conn(pool, conn) < 0) {
    log_errln("Error deleting connection.");
  }
  return -1;
}

// mark conn as err, not fatal yet.
// later will prepare err msg for it.
// return 1 always
static int liso_conn_err(conn_t* conn, int status) {
  conn->req->phase = REQ_ABORT;
  conn->resp->phase = RESP_ABORT;
  conn->resp->status = status;
  FD_SET(conn->fd, &pool->write_set);
  return 1;
}

// reset req/resp or close them.
// return 1 if conn is reset.
//       -1 if conn is drop.
static int liso_reset_or_close(conn_t* conn) {
  if (conn->req->alive) {

    pl_reset_conn(pool, conn);

    /* piped request */

    if (conn->req->last_buf->sz) {
      conn->buf->sz = conn->req->last_buf->sz;
      memcpy(conn->buf->data, conn->req->last_buf->data,
             conn->req->last_buf->sz);
      buf_reset(conn->req->last_buf);
      cn_parse_req(conn, conn->buf->data, liso_conn_err);

      if (conn->req->phase == REQ_DONE) {
        FD_CLR(conn->fd, &pool->read_set);
        FD_SET(conn->fd, &pool->write_set);
      }
    }

    return 1;

  } else {
    return liso_drop_conn(conn);
  }
}

// prepare to recv stderr from cgi
static int liso_cgi_inited(conn_t* conn) {
  FD_SET(conn->cgi->srv_err, &pool->read_set);
  return 1;
}

#define liso_recv(conn)                     \
  cn_recv(conn, liso_conn_err, liso_drop_conn)

#define liso_prepare_static_header(conn)    \
  cn_prepare_static_header(conn, &conf, liso_conn_err)

#define liso_serve_static(conn)             \
  cn_serve_static(conn, liso_reset_or_close, liso_drop_conn)

#define liso_init_cgi(conn)                 \
  cn_init_cgi(conn, &conf, liso_cgi_inited, liso_conn_err)

#define liso_stream_to_cgi(conn)            \
  cn_stream_to_cgi(conn, liso_conn_err)

#define liso_stream_from_cgi(conn)          \
  cn_stream_from_cgi(conn, liso_conn_err)

#define liso_serve_dynamic(conn)            \
  cn_serve_dynamic(conn, liso_reset_or_close, liso_drop_conn)


int main(int argc, char* argv[]) {

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

  // open listener sockets
  sock = open_listener_socket(conf.http_port);
  ssl_sock = open_listener_socket(conf.https_port);

  // create ssl context
  ssl_ctx = new_ssl_ctx(conf.prv, conf.crt);

  // init conn pool
  pool = pl_new(sock, ssl_sock);

  // daemonize server
  daemonize(conf.lock);

  // avoid crash when client continues to send after sock is closed.
  signal(SIGPIPE, SIG_IGN);
  // set up signal handlers
  signal(SIGCHLD, signal_handler); /* child terminate signal */
  signal(SIGHUP, signal_handler);  /* hangup signal */
  signal(SIGTERM, signal_handler); /* software termination signal from kill */

  /* setup log */
  log_init(conf.log);
  log_line("-------- Liso Server starts --------");

  /******** main loop ********/

  while (1) {

    // get pool ready
    pl_ready(pool);

    // select those who are ready
    if ((pool->n_ready = select(pool->max_fd+1,
                                &pool->read_ready,
                                &pool->write_ready,
                                NULL, NULL)) == -1) {
      log_errln("[select] %s", strerror(errno));
      errno = 0;
      continue;
    }

#if DEBUG >= 2
    log_line("[select] n_ready=%zu", pool->n_ready);
    for (i = 0; i < pool->n_conns; i++) {
      conn_t* conn = pool->conns[i];
      if (FD_ISSET(conn->fd, &pool->read_ready))
        log_line("[select] %d is ready to read.", conn->fd);
      if (FD_ISSET(conn->fd, &pool->write_ready))
        log_line("[select] %d is ready to write.", conn->fd);
      if (conn->cgi->srv_in > 0 &&
          FD_ISSET(conn->cgi->srv_in, &pool->read_ready))
        log_line("[select] cgi resp %d is ready to read.",
                 conn->cgi->srv_in);
    }
#endif

    /**** new connection ****/

    if (FD_ISSET(sock, &pool->read_ready)) {
      if (liso_accept_conn(sock, NULL)) {
#if DEBUG >= 1
        log_line("[main loop] accept conn from %d.", conf.http_port);
#endif
      }
    }

    if (FD_ISSET(ssl_sock, &pool->read_ready)) {
      if (liso_accept_conn(ssl_sock, ssl_ctx)) {
#if DEBUG >= 1
        log_line("[main loop] accept conn from %d.", conf.https_port);
#endif
      }
    }

    /**** serve connections ****/

    int max_fd = pool->min_max_fd;

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

      // handle cgi err
      if (conn->cgi->srv_err >= 0 &&
          FD_ISSET(conn->cgi->srv_err, &pool->read_ready)) {
        cgi_logerr(conn->cgi);
      }

      /* transitions */

      if (conn->req->type == REQ_STATIC &&
          conn->req->phase == REQ_DONE &&
          conn->resp->phase == RESP_READY) {
        // will set phase inside; only prepare once.
        liso_prepare_static_header(conn);
      }

      if (conn->req->type == REQ_DYNAMIC &&
          conn->cgi->phase == CGI_READY) {
        liso_init_cgi(conn);
      }

      // prepare for select
      if (conn->req->phase == REQ_DONE) {

        FD_CLR(conn->fd, &pool->read_set);

        // static response is fast, so ready for write now
        if (conn->req->type == REQ_STATIC)
          FD_SET(conn->fd, &pool->write_set);

        // dynamic response needs to make srv_in ready first
        if (conn->req->type == REQ_DYNAMIC)
          FD_SET(conn->cgi->srv_in, &pool->read_set);
      }

      if (conn->req->type == REQ_DYNAMIC &&
          conn->cgi->phase == CGI_SRV_TO_CGI) {

        // assumes we can stream conn->buf->data_p
        // to cgi pipe without blocking
        liso_stream_to_cgi(conn);

        // cgi stream out/in transition
        if (conn->req->phase == REQ_DONE) {
          close_pipe(&conn->cgi->srv_out);
          conn->cgi->phase = CGI_CGI_TO_SRV;
        }
      }

      /* serve */

      if (conn->req->type == REQ_DYNAMIC &&
          conn->cgi->phase == CGI_CGI_TO_SRV) {

        if (FD_ISSET(conn->cgi->srv_in, &pool->read_ready) &&
            conn->cgi->buf_phase == BUF_RECV) {
          liso_stream_from_cgi(conn);
        }

        // late write ready to prevent busy waiting
        if (FD_ISSET(conn->cgi->srv_in, &pool->read_ready) &&
            !FD_ISSET(conn->fd, &pool->write_set))
          FD_SET(conn->fd, &pool->write_set);

        if (FD_ISSET(conn->fd, &pool->write_ready) &&
            conn->cgi->buf_phase == BUF_SEND) {
          if (liso_serve_dynamic(conn) < 0) {
            i -= 1;
            continue;
          }
        }
      }

      // both static/dynamic request can go this flow
      // 1. serving static request
      // 2. bad request
      // 3. cgi error
      if (FD_ISSET(conn->fd, &pool->write_ready) &&
          conn->resp->phase != RESP_DISABLED) {
        if (liso_serve_static(conn) < 0) {
          i -= 1;
          continue;
        }
      }

      /* maintain max_fd */
      max_fd = max(max_fd, conn->fd);
      max_fd = max(max_fd, conn->cgi->srv_in);
      max_fd = max(max_fd, conn->cgi->srv_err);

      if (conn->fd >= FD_SETSIZE)
        log_errln("conn->fd %d is getting too large!", conn->fd);
      if (conn->cgi->srv_in >= FD_SETSIZE) {
        log_errln("conn->cgi->srv_in %d is getting too large!",
                  conn->cgi->srv_in);
        log_errln("cgi phase is %d.", conn->cgi->phase);
      }
      if (conn->cgi->srv_err >= FD_SETSIZE)
        log_errln("conn->cgi->srv_err %d is getting too large!",
                  conn->cgi->srv_err);
    }

    // update max_fd
    pool->max_fd = max_fd;

#if DEBUG >= 1
    log_line("[main loop] max fd is %d", pool->max_fd);
#endif
  }

  return teardown(EXIT_SUCCESS);
}
