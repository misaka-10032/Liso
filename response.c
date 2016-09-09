/**
 * @file errors.c
 * @brief Implementation of errors.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <sys/socket.h>
#include "response.h"
#include "io.h"

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
