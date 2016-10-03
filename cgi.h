/**
 * @file cgi.h
 * @brief Common Gateway Interface.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef CGI_H
#define CGI_H

#include "request.h"
#include "io.h"
#include "config.h"

typedef struct {
  enum {
    CGI_IDLE=1,
    CGI_READY,
    CGI_SRV_TO_CGI,
    CGI_CGI_TO_SRV,
    CGI_DONE,
    CGI_ABORT,
    CGI_DISABLED,
  } phase;

  int srv_out, srv_in;
  int cgi_in, cgi_out;

  enum {
    BUF_RECV=1,
    BUF_SEND,
  } buf_phase;
} cgi_t;

cgi_t* cgi_new();
void cgi_free(cgi_t* cgi);
void cgi_reset(cgi_t* cgi);
bool cgi_init(cgi_t* cgi, const req_t* req, const conf_t* conf);
void close_pipe(int* fd);

#endif // CGI_H
