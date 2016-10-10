/**
 * @file request.c
 * @brief Implementation of request.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include <string.h>
#include <strings.h>
#include "request.h"
#include "logging.h"
#include "utils.h"

req_t* req_new() {
  req_t* req = malloc(sizeof(req_t));
  req->scheme = HTTP;  // scheme won't change in a conn
  req->hdrs = hdr_new(NULL, NULL);
  req->last_buf = buf_new();
  req_reset(req);
  return req;
}

void req_reset(req_t* req) {
  req->method = M_OTHER;
  req->uri[0] = 0;
  req->params = NULL;
  req->version[0] = 0;
  req->host[0] = 0;
  req->clen = -1;
  req->rsize = 0;
  req->alive = true;
  hdr_reset(req->hdrs);
  req->phase = REQ_START;
  req->type = REQ_STATIC;
  // don't reset last_buf here
}

void req_free(req_t* req) {
  hdr_free(req->hdrs);
  buf_free(req->last_buf);
  free(req);
}

// Checks if it's a space except for \n.
#define issp(c) ((c) == ' '  || \
                 (c) == '\t' || \
                 (c) == '\r' || \
                 (c) == '\v' || \
                 (c) == '\f')

// This macro moves p and buf->data_p forward,
// such that [p, buf->data_p) wraps around a
// stripped string within a line.
#define proceed_inline() {                  \
  while (buf->data_p < buf_end(buf) &&      \
         issp(*(char*) buf->data_p))        \
    buf->data_p++;                          \
  p = (char*) buf->data_p;                  \
  while (buf->data_p < buf_end(buf) &&      \
         !isspace(*(char*) buf->data_p))    \
    buf->data_p++;                          \
}

// This macro moves p and buf->data_p forward,
// such that [p, buf->data_p) wraps around a
// stripped line.
#define proceed_line() {                    \
  while (buf->data_p < buf_end(buf) &&      \
         issp(*(char*) buf->data_p))        \
    buf->data_p++;                          \
  p = (char*) buf->data_p;                  \
  while (buf->data_p < buf_end(buf) &&      \
         *(char*) buf->data_p != '\n')      \
    buf->data_p++;                          \
}

// Checks if the current posisiton by p and
// buf->data_p is end of line.
#define eol() (p == (char*) buf->data_p  && \
               p < (char*) buf_end(buf) &&  \
               p[-1] == '\r' && p[0]  == '\n')

// This parser is able to parse multiple lines, including the first
// request line and the following header lines. To use this function,
// just feed in as many lines as possible. Parser will start parsing
// from buf->data_p. Caller should make sure buf ends with \n.
ssize_t req_parse(req_t* req, buf_t* buf) {
  char* p;
  /******** phase START ********/
  if (req->phase == REQ_START) {
    /* parse method */
    proceed_inline();
    char method[9];
    strncpy0(method, p, min(8, (char*) buf->data_p-p));

#if DEBUG >= 2
    log_line("[req_parse] Method size: %zd", (char*) buf->data_p-p);
    log_line("[req_parse] Parsed method: %s", method);
#endif

    if (strlen(method) == 0) {
      req->phase = REQ_ABORT;
      return -400;  // malformed header
    }

    if (!strncasecmp(method, "GET", 3) && issp(p[3]))
      req->method = M_GET;
    else if (!strncasecmp(method, "HEAD", 4) && issp(p[4]))
      req->method = M_HEAD;
    else if (!strncasecmp(method, "POST", 4) && issp(p[4]))
      req->method = M_POST;
    else {
#if DEBUG >= 1
      log_line("[req_parse] Method not supported: %s", method);
#endif
      req->phase = REQ_ABORT;
      return -501;  // method not supported
    }

    /* parse uri */
    proceed_inline();
    *req->uri = 0;
    strncpy0(req->uri, p, min(REQ_URISZ, (char*) buf->data_p-p));
#if DEBUG >= 2
    log_line("[req_parse] Parsed uri: %s", req->uri);
#endif

    char* host_start = NULL;
    char* host_end = NULL;
    if (!strcasecmp(req->uri, "http://"))
      host_start = req->uri + 8;
    else if (!strcasecmp(req->uri, "https://"))
      host_start = req->uri + 9;
    if (host_start)
      host_end = strchr(host_start, '/');
    if (host_start && host_end) {
      strncpy0(req->host, host_start, host_end-host_start);
      strcpy0(req->uri, host_end);
    }

#if DEBUG >= 2
    log_line("[req_parse] Processed uri: %s", req->uri);
#endif

    // check cgi
    if (strstartswith(req->uri, "/cgi/"))
      req->type = REQ_DYNAMIC;

    // separate uri and params
    req->params = strchr(req->uri, '?');
    if (req->params)
      *req->params++ = 0;

    /* parse version */
    proceed_inline();
    strncpy0(req->version, p, min(REQ_VERSZ, (char*) buf->data_p-p));
#if DEBUG >= 2
    log_line("[req_parse] Parsed version: %s", req->version);
#endif

    /* goto new line */
    proceed_inline();
    if (!eol()) {
#if DEBUG >= 1
      log_line("[req_parse] Met %c%c; \\r\\n expected.", p[-1], p[0]);
#endif
      req->phase = REQ_ABORT;
      return -400;  // malformed header
    }

    buf->data_p++;
    req->phase = REQ_HEADER;
  }

  /******** phase HEADER ********/
  if (req->phase == REQ_HEADER) {
    while (buf->data_p < buf_end(buf)) {

      proceed_line();

      /**** finished parsing ****/
      if (eol()) {
        buf->data_p++;
        req->phase = REQ_BODY;

        if (req->clen < 0) {
          if (req->method == M_POST) {
            req->phase = REQ_ABORT;
            return -411;  // clen needed
          } else {
            req->clen = 0;
          }
        }

        req->rsize = req->clen;
        break;
      }

      char* q;
      char key[HDR_KEYSZ+1];
      char val[HDR_VALSZ+1];
      for (q = p; q < (char*) buf->data_p && *q != ':'; q++);
      if (q == p || *q != ':') {
#if DEBUG >= 1
        log_line("[req_parse] No : is found.");
#endif
        req->phase = REQ_ABORT;
        return -400;  // malformed header
      }
      strncpy0(key, p, min(HDR_KEYSZ, q-p));
      strncpy0(val, q+1, min(HDR_VALSZ, (char*) buf->data_p-q-1));
      strstrip(key); strstrip(val);
#if DEBUG >= 2
      log_line("[req_parse] key=%s, val=%s", key, val);
#endif

      if (!strcasecmp(key, "Host")) {
        strcpy0(req->host, val);

      } else if (!strcasecmp(key, "Content-Length")) {
        if (isnum(val))
          req->clen = atoi(val);
        else {
#if DEBUG >= 1
          log_line("[req_parse] Invalid %s: %s", key, val);
#endif
          req->phase = REQ_ABORT;
          return -400;  // malformed header
        }

      } else if (!strcasecmp(key, "Connection")) {
        if (!strcasecmp(val, "close")) {
          req->alive = false;
        }

      } else {
        hdr_insert(req->hdrs, hdr_new(key, val));
      }

      // move to new line
      buf->data_p++;
    }
  }
  return buf->data_p - buf->data;
}

#define pack_next(sp) {                  \
  buf->data_p += strlen(buf->data_p);    \
  *(char*) buf->data_p++ = sp;           \
  *(char*) buf->data_p = 0;              \
}

ssize_t req_pack(req_t* req, buf_t* buf) {
  buf->data_p = buf->data;
  *(char*) buf->data_p = 0;

  /* pack method */
  strcpy0(buf->data_p, req_method(req));
  pack_next(' ');

  /* pack uri */
  strcpy0(buf->data_p, req->uri);
  pack_next(' ');

  /* pack version */
  strcpy0(buf->data_p, req->version);
  pack_next('\r'); pack_next('\n');

  /* pack host */
  strcpy0(buf->data_p, "Host:");
  pack_next(' ');
  strcpy0(buf->data_p, req->host);
  pack_next('\r'); pack_next('\n');

  /* pack clen */
  strcpy0(buf->data_p, "Content-Length:");
  pack_next(' ');
  sprintf(buf->data_p, "%zd", req->clen);
  pack_next('\r'); pack_next('\n');

  /* pack headers */
  hdr_t* hdr;
  for (hdr = req->hdrs->next; hdr; hdr = hdr->next) {
    strcpy0(buf->data_p, hdr->key);
    pack_next(':'); pack_next(' ');
    strcpy0(buf->data_p, hdr->val);
    pack_next('\r'); pack_next('\n');
  }

  /* pack end */
  pack_next('\r'); pack_next('\n');
  buf->sz = buf->data_p - buf->data;
  buf->data_p = buf->data;
  return buf->sz;
}

const char* req_method(const req_t* req) {
  switch(req->method) {
    case M_GET:
      return "GET";
    case M_HEAD:
      return "HEAD";
    case M_POST:
      return "POST";
    case M_OTHER:
      return "OTHER";
  }
  return NULL;
}
