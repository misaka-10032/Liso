/**
 * @file cgi.c
 * @brief Implementation of cgi.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <errno.h>
#include "cgi.h"
#include "logging.h"

#define ENVP_CNT 64
#define ENVSZ 2048

cgi_t* cgi_new() {
  cgi_t* cgi = malloc(sizeof(cgi_t));
  cgi->srv_in = -1;
  cgi->srv_out = -1;
  cgi->cgi_in = -1;
  cgi->cgi_out = -1;
  cgi_reset(cgi);
  return cgi;
}

void cgi_free(cgi_t* cgi) {
  if (cgi) {
    free(cgi);
  }
}

void close_pipe(int* fd) {
  if (*fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

void cgi_reset(cgi_t* cgi) {
  cgi->phase = CGI_IDLE;

  close_pipe(&cgi->srv_out);
  close_pipe(&cgi->srv_in);
  close_pipe(&cgi->cgi_in);
  close_pipe(&cgi->cgi_out);

  cgi->buf_phase = BUF_RECV;
}

static char* str_new(char* src, int sz) {
  char* str = malloc(sz+1);
  strncpy0(str, src, sz);
  return str;
}

#define add_entry(fmt, arg1) {                 \
  sz = snprintf(data, ENVSZ, fmt, arg1);       \
  envp[cnt++] = str_new(data, sz);             \
}

#define add_entry_kv(fmt, arg1, arg2) {        \
  sz = snprintf(data, ENVSZ, fmt, arg1, arg2); \
  envp[cnt++] = str_new(data, sz);             \
}

static void convert_key(const char* from, char* to) {
  *to++ = 'H'; *to++ = 'T'; *to++ = 'T'; *to++ = 'P'; *to++ = '_';
  int i;
  for (i = 0; *from && i < HDR_KEYSZ; i++, from++, to++) {
    if (*from >= 'a' && *from <='z')
      *to = *from - 'a' + 'A';
    else if (*from == '-')
      *to = '_';
    else
      *to = *from;
  }
  *to = 0;
}

// NOT thread safe
static char** envp_new(const req_t* req, const conf_t* conf) {
  static char data[ENVSZ+1];
  static char key[HDR_KEYSZ+5];
  char** envp = malloc(sizeof(char*) * ENVP_CNT);
  int cnt = 0;
  int sz;

  add_entry("GATEWAY_INTERFACE=%s", "CGI/1.1");
  add_entry("REQUEST_URI=%s", req->uri);
  add_entry("REQUEST_METHOD=%s", req_method(req));

  if (req->params)
    add_entry("QUERY_STRING=%s", req->params);

  if (req->clen > 0)
    add_entry("Content-Length=%zd", req->clen);

  // TODO: remote addr

  add_entry("SERVER_NAME=%s", VERSION);
  add_entry("SERVER_SOFTWARE=%s", VERSION);
  add_entry("SERVER_PROTOCOL=%s", "HTTP/1.1");
  add_entry("HTTP_HOST=%s", req->host);
  add_entry("SCRIPT_NAME=%s", conf->cgi);

  if (req->scheme == HTTPS)
    add_entry("HTTPS=%s", "on");

  hdr_t* hdr = req->hdrs->next;
  for (hdr = req->hdrs->next;
       hdr && cnt < ENVP_CNT;
       hdr = hdr->next) {
    if (!strcasecmp(hdr->key, "Content-Type")) {
      add_entry("CONTENT_TYPE=%s", hdr->val);
    } else {
      convert_key(hdr->key, key);
      add_entry_kv("%s=%s", key, hdr->val);
    }
  }

  envp[cnt] = NULL;

#if DEBUG >= 2
  int i;
  for (i = 0; i < cnt; i++)
    log_line("[envp_new] %s", envp[i]);
#endif

  return envp;
}

static void envp_free(char** envp) {
  int i;
  for (i = 0; envp[i]; i++)
    free(envp[i]);
  free(envp);
}

bool cgi_init(cgi_t* cgi, const req_t* req, const conf_t* conf) {

  int stdin_pipe[2];
  int stdout_pipe[2];
  if (pipe(stdin_pipe) < 0)
    return false;
  if (pipe(stdout_pipe) < 0)
    return false;

  cgi->srv_in = stdout_pipe[0];
  cgi->srv_out = stdin_pipe[1];
  cgi->cgi_in = stdin_pipe[0];
  cgi->cgi_out = stdout_pipe[1];

#if DEBUG >= 1
  log_line("[CGI init] srv_in is %d.", cgi->srv_in);
  log_line("[CGI init] srv_out is %d.", cgi->srv_out);
  log_line("[CGI init] cgi_in is %d.", cgi->cgi_in);
  log_line("[CGI init] cgi_out is %d.", cgi->cgi_out);
#endif

  int pid = fork();
  if (pid < 0)
    return false;

  /**** child ****/
  if (pid == 0) {
    char* argv[] = {conf->cgi, NULL};
    char** envp = envp_new(req, conf);

    close_pipe(&cgi->srv_in);
    close_pipe(&cgi->srv_out);

    if (dup2(cgi->cgi_in, STDIN_FILENO) < 0) {
      log_errln("[CGI dup2 stdin] %s", strerror(errno));
      errno = 0;
    }

    if (dup2(cgi->cgi_out, STDOUT_FILENO) < 0) {
      log_errln("[CGI dup2 stdout] %s", strerror(errno));
      errno = 0;
    }

    // TODO stderr

#if DEBUG >= 1
    int cnt;
    for (cnt = 0; envp[cnt]; cnt++);
    log_line("[CGI] execve, file=%s, envp_cnt=%d",
             conf->cgi, cnt);
    log_flush();
#endif

    if (execve(conf->cgi, argv, envp) < 0) {
      log_errln("[CGI] %s", strerror(errno));
      log_flush();
      envp_free(envp);
      errno = 0;
      close_pipe(&cgi->cgi_in);
      close_pipe(&cgi->cgi_out);
      exit(EXIT_FAILURE);
    }

    // execve doesn't return, so don't need to free envp.
  }

  /**** parent ****/
  if (pid > 0) {

#if DEBUG >= 1
    log_line("[CGI] forked cgi %d.", pid);
    log_flush();
#endif

    close_pipe(&cgi->cgi_in);
    close_pipe(&cgi->cgi_out);
    cgi->phase = CGI_SRV_TO_CGI;
  }

  return true;
}

