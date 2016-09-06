/******************************************************************************
* errors.c                                                                    *
*                                                                             *
* Description: Implementation of errors                                       *
*                                                                             *
* Author(s):   Longqi Cai (longqic@andrew.cmu.edu)                            *
*                                                                             *
*******************************************************************************/

#include <sys/socket.h>
#include "errors.h"

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

void err_resp(int no, int fd) {
  char* msg = "";
  ssize_t msglen = 0;
  switch (no) {
  case 501:
    msg = err501;
    break;
  case 503:
    msg = err503;
    break;
  default:
#if DEBUG >= 1
    fprintf(stderr, "Error(%d) undefined.\n", no);
#endif
    break;
  }
  msglen = strlen(msg);
  if (send(fd, msg, msglen, 0) != msglen) {
    fprintf(stderr, "Error sending error msg.\n");
  }
}
