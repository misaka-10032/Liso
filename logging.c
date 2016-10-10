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
#define SEM "longqic_log"

// file descriptor for the log
static int fd = -1;
// line buffer
static char dt[DATESZ];
static char line[LINESZ+1];
static char wrapped[LINESZ+DATESZ+10];

static void lock() {
  lockf(fd, F_LOCK, 0);
}

static void unlock() {
  lockf(fd, F_ULOCK, 0);
}

void log_init(char* fname) {
  if (fd < 0) {
    fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0640);
  }
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

  lock();

  va_list args;
  va_start(args, fmt);
  vsprintf(line, fmt, args);
  va_end(args);

  prepare_datetime(dt);
  sprintf(wrapped, "%s - %s\n", dt, line);
  write(fd, wrapped, strlen(wrapped));

  unlock();
}

void log_errln(char* fmt, ...) {

  lock();

  va_list args;
  va_start(args, fmt);
  vsprintf(line, fmt, args);
  va_end(args);

  prepare_datetime(dt);
  sprintf(wrapped, "%s !!! ERROR !!! %s\n", dt, line);
  write(fd, wrapped, strlen(wrapped));

  unlock();
}

void log_raw(void* data, size_t n) {
  lock();
  write(fd, data, n);
  write(fd, "\n", 1);
  unlock();
}

void log_flush() {
  fsync(fd);
}

void log_stop() {
  if (fd > 0) {
    close(fd);
  }
}
