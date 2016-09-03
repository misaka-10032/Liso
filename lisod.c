/******************************************************************************
* lisod.c                                                                     *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Author(s):   Longqi Cai <longqic@andrew.cmu.edu>                            *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "pool.h"
#include "io.h"
#include "utils.h"

int close_socket(int sock) {
  if (close(sock)) {
    fprintf(stderr, "Failed closing socket.\n");
    return 1;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  int sock, client_sock;
  socklen_t cli_size;
  struct sockaddr_in addr, cli_addr;
  pool_t* pool;
  int i;

  const int ARG_CNT = 8;
  int http_port;
  int https_port;
  char* log_file;
  char* lock_file;
  char* www_folder;
  char* cgi_path;
  char* prvkey_file;
  char* cert_file;

  if (argc != ARG_CNT+1) {
    fprintf(stdout, "Usage: %s <HTTP port> <HTTPS port> <log file>"
                    "<lock file> <www folder> <CGI script path>"
                    "<private key file> <certificate file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  http_port = atoi(argv[1]);
  https_port = atoi(argv[2]);
  log_file = argv[3];
  lock_file = argv[4];
  www_folder = argv[5];
  cgi_path = argv[6];
  prvkey_file = argv[7];
  cert_file = argv[8];

  fprintf(stdout, "----- Liso Server -----\n");
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
      return EXIT_FAILURE;
    }

    /*** new connection ***/
    if (FD_ISSET(sock, &pool->read_ready)) {
      cli_size = sizeof(cli_addr);
      if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                &cli_size)) == -1) {
        close(sock);
        fprintf(stderr, "Error accepting connection.\n");
        return EXIT_FAILURE;
      }

      pool->max_fd = max(pool->max_fd, client_sock);
      conn_t* conn = cn_new(client_sock, pool->n_conns);
      pl_add_conn(pool, conn);
    }

    for (i = 0; i < pool->n_conns; i++) {
      conn_t* conn = pool->conns[i];
      buf_t* buf = conn->buf;
      if (!FD_ISSET(conn->fd, &pool->read_ready))
        continue;
      buf->sz = recv(conn->fd, buf->data, BUFSZ, 0);
      if (buf->sz < 0) {
        close_socket(conn->fd);
        close_socket(sock);
        fprintf(stderr, "Error receiving from client.\n");
        return EXIT_FAILURE;
      }
      if (buf->sz > 0) {
#ifdef DEBUG
        printf("[recv] from %d\n", conn->fd);
        fwrite(buf->data, buf->sz, 1, stdout);
        printf("\n");
#endif
        if ((send(conn->fd, buf->data, buf->sz, 0) != buf->sz)) {
          close_socket(conn->fd);
          close_socket(sock);
          fprintf(stderr, "Error sending to client.\n");
          return EXIT_FAILURE;
        }
      } else if (buf->sz == 0) {
#ifdef DEBUG
        printf("[recv end] from %d\n", conn->fd);
#endif
        pl_del_conn(pool, conn);
      }
    }
  }

  pl_free(pool);
  close_socket(sock);
  return EXIT_SUCCESS;
}
