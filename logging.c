/**
 * @file logging.c
 * @brief Implementation of log.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <semaphore.h>
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
#define SEM "liso_log"

// mutex
static sem_t* sem;
// file descriptor for the log
static int fd = -1;
// line buffer
static char dt[DATESZ];
static char line[LINESZ+1];
static char wrapped[LINESZ+DATESZ+10];

static void lock() {
  sem_wait(sem);
}

static void unlock() {
  sem_post(sem);
}

void log_init(char* fname) {
  if (fd < 0) {
    fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0640);
    sem = sem_open(SEM, O_CREAT);
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

void log_flush() {
  fsync(fd);
}

void log_stop() {
  if (fd > 0) {
    close(fd);
    sem_unlink(SEM);
  }
}
