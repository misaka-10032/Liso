/**
 * @file log.c
 * @brief Implementation of log.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "log.h"

// file descriptor for the log
static int fd;

void log_init(char* fname) {
  fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0640);
}

void prepare_datetime(char* datetime) {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  strftime(datetime, DTSZ, "%X %a %x", &tm);
}

void log_line(char* fmt, ...) {
  char dt[DTSZ]; prepare_datetime(dt);
  char line[LINESZ+1] = {0};
  char wrapped[LINESZ+DTSZ+10] = {0};

  va_list args;
  va_start(args, fmt);
  vsprintf(line, fmt, args);
  va_end(args);

  sprintf(wrapped, "%s - %s\n", dt, line);
  printf("%s", wrapped);
  write(fd, wrapped, strlen(wrapped));
}

void log_stop() {
  close(fd);
}
