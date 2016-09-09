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

int close_socket(int sock) {
#if DEBUG >= 1
  log_line("[close_socket] %d", sock);
#endif
  if (close(sock)) {
    fprintf(stderr, "Failed closing socket %d.\n", sock);
    return 1;
  }
  return 0;
}

// clean up the connection
void cleanup(pool_t* pool, conn_t* conn) {
  close_socket(conn->fd);
  if (pl_del_conn(pool, conn) < 0) {
    fprintf(stderr, "Error deleting connection.\n");
  }
}

void teardown() {
  close_socket(sock);
  log_line("-------- Liso Server stops --------");
  log_stop();
  exit(EXIT_SUCCESS);
}

void signal_handler(int sig) {
  switch (sig) {
    case SIGINT:
    case SIGTERM:
      teardown();
      break;
  }
}

int main(int argc, char* argv[]) {
  int client_sock;
  socklen_t cli_size;
  struct sockaddr_in addr, cli_addr;
  pool_t* pool;
  int i;

  const int ARG_CNT = 8;
  int http_port;
//  int https_port;
  char* log_file;
//  char* lock_file;
//  char* www_folder;
//  char* cgi_path;
//  char* prvkey_file;
//  char* cert_file;

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
//  www_folder = argv[5];
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
    return EXIT_FAILURE;
  }
  pool = pl_new(sock);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(http_port);
  addr.sin_addr.s_addr = INADDR_ANY;
  /* servers bind sockets to ports---notify the OS they accept connections */
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr))) {
    close_socket(sock);
    fprintf(stderr, "Failed binding socket.\n");
    return EXIT_FAILURE;
  }

  if (listen(sock, 5)) {
    close_socket(sock);
    fprintf(stderr, "Error listening on socket.\n");
    return EXIT_FAILURE;
  }

  /* main loop */
  while (1) {
    pl_ready(pool);
    /* select those who are ready */
    if ((pool->n_ready = select(pool->max_fd+1,
                                &pool->read_ready,
                                &pool->write_ready,
                                NULL, NULL)) == -1) {
      close(sock);
      fprintf(stderr, "Error in select.\n");
      continue;
    }
#if DEBUG >= 2
    log_line("[select] n_ready=%zu", pool->n_ready);
#endif

    /*** new connection ***/
    if (FD_ISSET(sock, &pool->read_ready)) {
      cli_size = sizeof(cli_addr);
      if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                &cli_size)) == -1) {
        fprintf(stderr, "Error in accept: %s\n", strerror(errno));
        errno = 0;
        continue;
      }

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

    for (i = 0; i < pool->n_conns; i++) {
      conn_t* conn = pool->conns[i];
      buf_t* buf = conn->buf;
      if (!FD_ISSET(conn->fd, &pool->read_ready))
        continue;
      buf->data_p = buf->data;
      buf->sz = recv(conn->fd, buf->data, BUFSZ, 0);
      if (buf->sz < 0) {
        cleanup(pool, conn);
        fprintf(stderr, "Error in recv: %s\n", strerror(errno));
        errno = 0;
        continue;
      }
      if (buf->sz > 0) {
#if DEBUG >= 3
        log_line("[recv] from %d", conn->fd);
#endif
        ssize_t rc;
#if DEBUG >= 1
        log_line("[main loop] parse req for %d", conn->fd);
#endif
        rc = req_parse(conn->req, buf);

        // handle bad header
        if (rc < 0) {
          if (rc == -1) {
            resp_err(501, conn->fd);
          } else {
            resp_err(400, conn->fd);
          }
          cleanup(pool, conn);
          continue;
        }

        if (conn->req->phase == BODY) {
          // TODO: stream body
          ssize_t size = buf_end(conn->buf) - conn->buf->data_p;
          conn->req->rsize -= size;
          if (conn->req->rsize <= 0) {
            conn->req->phase = DONE;
          }
        }

        if (conn->req->phase == DONE) {
          // TODO: ???
          buf_t* dump = buf_new();
          req_pack(conn->req, dump);
#if DEBUG >= 2
          log_line("[main loop] parsed req\n%s", dump->data);
#endif
          if ((send(conn->fd, dump->data, dump->sz, 0) != dump->sz)) {
            fprintf(stderr, "Error in send: %s\n", strerror(errno));
            errno = 0;
          }
          buf_free(dump);
          cleanup(pool, conn);
        }

      } else if (buf->sz == 0) {
#if DEBUG >= 1
        log_line("[recv end] from %d", conn->fd);
#endif
        cleanup(pool, conn);
        continue;
      }
    }
  }

  pl_free(pool);
  close_socket(sock);
  return EXIT_SUCCESS;
}
