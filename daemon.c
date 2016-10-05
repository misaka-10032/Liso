/**
 * @file daemon.c
 * @brief Implementation of daemon.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "daemon.h"

static int lfp = -1;

void daemonize(char* lock_file) {
  /* drop to having init() as parent */
  int i, pid = fork();
  char str[256] = {0};
  if (pid < 0) {
    fprintf(stderr, "Cannot fork daemon.\n");
    exit(EXIT_FAILURE);
  }

  /* parent */
  if (pid > 0) {
    for (i = getdtablesize(); i >= 0; i--)
      close(i);
    exit(EXIT_SUCCESS);
  }

  /* child*/
  setsid();
  pid = getpid();
  umask(027);

  lfp = open(lock_file, O_RDWR|O_CREAT, 0640);

  if (lfp < 0) {
    fprintf(stderr, "Cannot open lock file.\n");
    exit(EXIT_FAILURE);
  }

  if (lockf(lfp, F_TLOCK, 0) < 0) {
    fprintf(stderr, "Cannot lock the lock file.\n");
    exit(EXIT_FAILURE);
  }

  /* only first instance continues */
  int sz = sprintf(str, "%d\n", pid);
  write(lfp, str, sz); /* record pid to lockfile */

  printf("Successfully daemonized lisod, pid %d.\n", pid);

  // close stdin/out for pipe
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  open("/dev/null", O_RDONLY); /* stub stdin */
  open("/dev/null", O_WRONLY); /* stub stdout */
}

void release_lock() {
  if (lfp > 0)
    close(lfp);
}
