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

static const char title200[] = "200 OK";

static const char title400[] = "400 Bad Request";
static const char msg400[] =
"<html>" CRLF
"<head><title>400 Bad Request</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1>h1></center>center>" CRLF
;

static const char title404[] = "404 Not Found";
static const char msg404[] =
"<html>" CRLF
"<head><title>404 Not Found</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>404 Not Found</h1>h1></center>center>" CRLF
;

static const char title500[] = "500 Internal Server Error";
static const char msg500[] =
"<html>" CRLF
"<head><title>500 Internal Server Error</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>500 Internal Server Error</h1>h1></center>center>" CRLF
;

static const char title501[] = "501 Not Implemented";
static const char msg501[] =
"<html>" CRLF
"<head><title>501 Not Implemented</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>501 Not Implemented</h1>h1></center>center>" CRLF
;

static const char title503[] = "503 Service Unavailable";
static const char msg503[] =
"<html>" CRLF
"<head><title>503 Service Temporarily Unavailable</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>503 Service Temporarily Unavailable</h1>h1></center>center>" CRLF
;

resp_t* resp_new() {
  resp_t* resp = malloc(sizeof(resp_t));
  resp->mmbuf = NULL;
  resp_reset(resp);
  return resp;
}

void resp_reset(resp_t* resp) {
  resp->phase = RESP_READY;
  resp->status = 200;
  resp->clen = 0;
  resp->alive = true;
  mmbuf_free(resp->mmbuf);
  resp->mmbuf = NULL;
}

void resp_free(resp_t* resp) {

  if (resp->mmbuf)
    mmbuf_free(resp->mmbuf);

  free(resp);
}

static void get_ftype(char* path, char* ftype) {

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

  /* update header fields */
  resp->clen = s.st_size;
  get_ftype(path, resp->ftype);

  return s.st_size;
}

ssize_t resp_hdr(const resp_t* resp, char* hdr) {

  char* hdr_p = hdr;
  sprintf(hdr_p, "HTTP/1.1 %s\r\n", resp_title(resp->status));
  hdr_p += strlen(hdr_p);

  char date[RESP_DATESZ];
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  strftime(date, RESP_DATESZ, "%a, %d %b %Y %T %Z", &tm);

  sprintf(hdr_p, "Date: %s\r\n", date);
  hdr_p += strlen(hdr_p);

  // TODO: global version
  strcpy0(hdr_p, "Server: Liso/0.1.0\r\n");
  hdr_p += strlen(hdr_p);

  sprintf(hdr_p, "Connection: %s\r\n", resp->alive ? "keep-alive" : "close");
  hdr_p += strlen(hdr_p);

  sprintf(hdr_p, "Content-Length: %zd\r\n", resp->clen);
  hdr_p += strlen(hdr_p);

  sprintf(hdr_p, "Content-Type: %s\r\n", resp->ftype);
  hdr_p += strlen(hdr_p);

  strcpy0(hdr_p, "\r\n");
  hdr_p += strlen(hdr_p);

  return hdr_p - hdr;
}

const char* resp_title(int code) {
  switch (code) {
    case 200: return title200;
    case 400: return title400;
    case 404: return title404;
    case 501: return title501;
    case 503: return title503;
    default:
      fprintf(stderr, "Status Code(%d) undefined.\n", code);
      return title500;
  }
}

const char* resp_msg(int code) {
  switch (code) {
    case 400: return msg400;
    case 404: return msg404;
    case 501: return msg501;
    case 503: return msg503;
    default:
      fprintf(stderr, "Status Code(%d) undefined.\n", code);
      return msg500;
  }
}
