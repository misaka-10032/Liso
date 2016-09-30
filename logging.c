/**
 * @file logging.c
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
#include "logging.h"

#define DATESZ 64

// file descriptor for the log
static int fd = -1;
// line buffer
static char line[LINESZ+1];
static char wrapped[LINESZ+DATESZ+10];

void log_init(char* fname) {
  if (fd < 0)
    fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0640);
}

bool log_inited() {
  return fd > 0;
}

void prepare_datetime(char* datetime) {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  strftime(datetime, DATESZ, "%X %a %x", &tm);
}

void log_line(char* fmt, ...) {
  char dt[DATESZ]; prepare_datetime(dt);

  va_list args;
  va_start(args, fmt);
  vsprintf(line, fmt, args);
  va_end(args);

  sprintf(wrapped, "%s - %s\n", dt, line);
  write(fd, wrapped, strlen(wrapped));
}

void log_errln(char* fmt, ...) {
  char dt[DATESZ]; prepare_datetime(dt);

  va_list args;
  va_start(args, fmt);
  vsprintf(line, fmt, args);
  va_end(args);

  sprintf(wrapped, "%s !!! ERROR !!! %s\n", dt, line);
  write(fd, wrapped, strlen(wrapped));
}

void log_stop() {
  close(fd);
}
