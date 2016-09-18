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

static const char* default_pages[] = {
  "index.html",
  "index.htm"
};
#define n_default_pages (sizeof(default_pages) / sizeof(const char*))

// close a socket
int close_socket(int sock) {
  if (close(sock)) {
    fprintf(stderr, "Failed closing socket %d.\n", sock);
    return 1;
  }
  return 0;
}

// clean up the connection
static void cleanup(conn_t* conn) {
#if DEBUG >= 1
  log_line("[cleanup] %d.", conn->fd);
#endif
  int yes = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  close_socket(conn->fd);
  if (pl_del_conn(pool, conn) < 0) {
    fprintf(stderr, "Error deleting connection.\n");
  }
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

// switch from recv mode to send mode
// returns -1 if error is occurred.
//          1 if normal.
static int liso_recv_to_send(conn_t* conn) {
  int i;
  buf_t* buf = conn->buf;
  resp_t* resp = conn->resp;

  FD_CLR(conn->fd, &pool->read_set);
  FD_SET(conn->fd, &pool->write_set);

  char path[REQ_URISZ*3];
  strncpy0(path, www_folder, REQ_URISZ);
  strncat(path, conn->req->uri, REQ_URISZ);
#if DEBUG >= 1
  log_line("[recv_to_send] path is %s", path);
#endif

  bool mmapped = resp_mmap(resp, path) >= 0;

  if (!mmapped) {

    char* path_p = path + strlen(path);
    if (path_p[-1] != '/')
      *path_p++ = '/';

    for (i = 0; i < n_default_pages; i++) {
      strncpy0(path_p, default_pages[i], REQ_URISZ);
#if DEBUG >= 1
      log_line("[recv_to_send] try path %s", path);
#endif
      mmapped = resp_mmap(resp, path) >= 0;
      if (mmapped)
        break;
    }
  }

  if (!mmapped) {
    resp->phase = RESP_ABORT;
    cleanup(conn);
    return -1;
  }

  /* prepare header */
  buf_reset(buf);
  buf->sz = resp_hdr(resp, buf->data);
  conn->resp->phase = RESP_START;
  return 1;
}

// recv from conn
// returns -1 if error is encountered
//          0 if not ready
//          1 if it's normal.
static int liso_recv(conn_t* conn) {

  if (!FD_ISSET(conn->fd, &pool->read_ready))
    return 0;

  /******** recv msg ********/
  ssize_t rsize = BUFSZ - conn->buf->sz;

  if (rsize <= 0) {
    // header to large
    conn->req->phase = REQ_ABORT;
    // TODO: don't clean up here
    cleanup(conn);
    return -1;
  }

  // append to conn buf
  ssize_t dsize = recv(conn->fd, buf_end(conn->buf), rsize, 0);
  if (dsize < 0) {
    conn->req->phase = REQ_ABORT;
    cleanup(conn);
    fprintf(stderr, "Error in recv: %s\n", strerror(errno));
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
    cleanup(conn);
    return -1;
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

      ((char*)buf_lines->data)[buf_lines->sz] = 0;
      printf("!!! Lines are:\n%s", buf_lines->data);

      ssize_t rc;
      rc = req_parse(conn->req, buf_lines);

      buf_free(buf_lines);

      // TODO: do it later when write ready
      // handle bad header
      if (rc < 0) {
        conn->req->phase = REQ_ABORT;
        if (rc == -1) {
          resp_err(501, conn->fd);
        } else {
          resp_err(400, conn->fd);
        }
        cleanup(conn);
        return -1;
      }
    }
  }

  if (conn->req->phase == REQ_BODY) {
    // TODO: stream body
    // TODO: do I really need Content-Length?
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

// send to conn
// returns -1 if error is encountered.
//         0  if not ready.
//         1  if normal.
static int liso_send(conn_t* conn) {

  if (!FD_ISSET(conn->fd, &pool->write_ready))
    return 0;

  // send content in buf_dst.
  // then copy buf_src to buf_dst for the next chunk.
  buf_t* buf_src = conn->resp->mmbuf;
  buf_t* buf_dst = conn->buf;

  if (send(conn->fd, buf_dst->data, buf_dst->sz, 0) < 0) {
    conn->resp->phase = RESP_ABORT;
#if DEBUG >= 1
    log_line("[liso_send] Error when sending to %d.", conn->fd);
#endif
    cleanup(conn);
    return -1;
  }

#if DEBUG >= 2
  log_line("[liso_send] Sent %zd bytes to %d", buf_dst->sz, conn->fd);
#endif

  if (conn->req->method == M_HEAD) {
    conn->resp->phase = RESP_DONE;
#if DEBUG >= 1
    log_line("[liso_send] Finished HEAD response to %d.", conn->fd);
#endif
    cleanup(conn);
  }

  ssize_t sz = min(BUFSZ, buf_rsize(buf_src));
  if (sz == 0) {
    conn->resp->phase = RESP_DONE;
#if DEBUG >= 1
    log_line("[liso_send] Finished sending to %d.", conn->fd);
#endif
    cleanup(conn);
    return -1;
  }

  /* prepare for the next chunk */
  memcpy(buf_dst->data, buf_src->data_p, sz);
  buf_src->data_p += sz;
  buf_dst->sz = sz;
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
  /* all networked programs must create a socket */
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Failed creating socket.\n");
    teardown(EXIT_FAILURE);
  }

  /* init conn pool */
  pool = pl_new(sock);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(http_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  /* servers bind sockets to ports---notify the OS they accept connections */
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
    fprintf(stderr, "Failed binding socket.\n");
    teardown(EXIT_FAILURE);
  }

  if (listen(sock, 5)) {
    fprintf(stderr, "Error listening on socket.\n");
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
      fprintf(stderr, "Error in select.\n");
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
        fprintf(stderr, "Error in accept: %s\n", strerror(errno));
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
        fprintf(stderr, "Max conns reached; drop client %d.\n", client_sock);
        close_socket(client_sock);
        continue;
      }

      pool->max_fd = max(pool->max_fd, client_sock);
      conn_t* conn = cn_new(client_sock, pool->n_conns);

      if (pl_add_conn(pool, conn) < 0) {
        fprintf(stderr, "Error adding connection.\n");
        continue;
      }
    }

    /**** serve connections ****/
    for (i = 0; i < pool->n_conns; i++) {
      int rc;
      rc = liso_recv(pool->conns[i]);
      if (rc < 0) continue;
      rc = liso_send(pool->conns[i]);
    }
  }

  teardown(EXIT_SUCCESS);
}
