/**
 * @file errors.c
 * @brief Implementation of errors.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "response.h"
#include "io.h"
#include "utils.h"
#include "logging.h"

static char err400[] =
"<html>" CRLF
"<head><title>400 Bad Request</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1>h1></center>center>" CRLF
;

static char err501[] =
"<html>" CRLF
"<head><title>501 Not Implemented</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>501 Not Implemented</h1>h1></center>center>" CRLF
;

static char err503[] =
"<html>" CRLF
"<head><title>503 Service Temporarily Unavailable</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>503 Service Temporarily Unavailable</h1>h1></center>center>" CRLF
;

resp_t* resp_new() {
  resp_t* resp = malloc(sizeof(resp_t));
  resp->phase = RESP_READY;
  resp->mmbuf = NULL;
  return resp;
}

void resp_free(resp_t* resp) {

  if (resp->mmbuf)
    mmbuf_free(resp->mmbuf);

  free(resp);
}

void get_ftype(char* path, char* ftype) {

  if (caseendswith(path, ".html") ||
      caseendswith(path, ".htm"))
    strcpy0(ftype, "text/html");

  else if (caseendswith(path, ".css"))
    strcpy0(ftype, "text/css");

  else if (caseendswith(path, ".js"))
    strcpy0(ftype, "application/javascript");

  else if (caseendswith(path, "png"))
    strcpy0(ftype, "image/png");

  else if (caseendswith(path, ".jpg") ||
           caseendswith(path, ".jpeg"))
    strcpy0(ftype, "image/jpeg");

  else if (caseendswith(path, ".gif"))
    strcpy0(ftype, "image/gif");

  else
    strcpy0(ftype, "text/plain");
}

ssize_t resp_mmap(resp_t* resp, char* path) {

  int fd;
  struct stat s;

  if (stat(path, &s) < 0) {
#if DEBUG >= 1
    log_line("[resp_mmap] Error in stat path %s", path);
#endif
    return -1;
  }

  if (S_ISDIR(s.st_mode)) {
#if DEBUG >= 1
    log_line("[resp_mmap] Path is dir: %s", path);
#endif
    return -1;
  }

  if ((fd = open(path, O_RDONLY, 0)) < 0) {
#if DEBUG >= 1
    log_line("[resp_mmap] Error in open path %s", path);
#endif
    return -1;
  }

  resp->mmbuf = mmbuf_new(fd, s.st_size);
  if (!resp->mmbuf)
    return -1;

  close(fd);

  get_ftype(path, resp->ftype);

  return s.st_size;
}

size_t resp_hdr(const resp_t* resp, char* hdr) {

  char* hdr_p = hdr;
  strcpy0(hdr_p, "HTTP/1.1 200 OK\r\n");
  hdr_p += strlen(hdr_p);

  char date[RESP_DATESZ];
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  strftime(date, RESP_DATESZ, "%a, %d %b %Y %T %z", &tm);

  sprintf(hdr_p, "Date: %s\r\n", date);
  hdr_p += strlen(hdr_p);

  strcpy0(hdr_p, "Server: Liso/0.1.0\r\n");
  hdr_p += strlen(hdr_p);

  strcpy0(hdr_p, "Connection: Close\r\n");
  hdr_p += strlen(hdr_p);

  sprintf(hdr_p, "Content-Length: %zd\r\n", resp->mmbuf->sz);
  hdr_p += strlen(hdr_p);

  sprintf(hdr_p, "Content-Type: %s\r\n", resp->ftype);
  hdr_p += strlen(hdr_p);

  strcpy0(hdr_p, "\r\n");
  hdr_p += strlen(hdr_p);

  return hdr_p - hdr;
}

void resp_err(int code, int fd) {

  char* msg = "";
  ssize_t msglen = 0;

  switch (code) {
    case 400: msg = err400; break;
    case 501: msg = err501; break;
    case 503: msg = err503; break;
    default:
      fprintf(stderr, "Error(%d) undefined.\n", code);
      break;
  }

  msglen = strlen(msg);
  if (send(fd, msg, msglen, 0) != msglen) {
    fprintf(stderr, "Error sending error msg.\n");
  }
}
