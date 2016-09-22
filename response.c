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

/**** Special responses ****/

static const char title200[] = "200 OK";

static const char title400[] = "400 Bad Request";
static const char msg400[] =
"<html>" CRLF
"<head><title>400 Bad Request</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"</body>" CRLF
"</html>" CRLF;

static const char title404[] = "404 Not Found";
static const char msg404[] =
"<html>" CRLF
"<head><title>404 Not Found</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>404 Not Found</h1></center>" CRLF
"</body>" CRLF
"</html>" CRLF;

static const char title411[] = "411 Length Required";
static const char msg411[] =
"<html>" CRLF
"<head><title>411 Length Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>411 Length Required</h1></center>" CRLF
"</body>" CRLF
"</html>" CRLF;

static const char title500[] = "500 Internal Server Error";
static const char msg500[] =
"<html>" CRLF
"<head><title>500 Internal Server Error</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>500 Internal Server Error</h1></center>" CRLF
"</body>" CRLF
"</html>" CRLF;

static const char title501[] = "501 Not Implemented";
static const char msg501[] =
"<html>" CRLF
"<head><title>501 Not Implemented</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>501 Not Implemented</h1>h1></center>center>" CRLF
"</body>" CRLF
"</html>" CRLF;

static const char title503[] = "503 Service Unavailable";
static const char msg503[] =
"<html>" CRLF
"<head><title>503 Service Temporarily Unavailable</title>title></head>head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>503 Service Temporarily Unavailable</h1>h1></center>center>" CRLF
"</body>" CRLF
"</html>" CRLF;

/**** Default pages if not specified ****/

static const char* default_pages[] = {
  "index.html",
  "index.htm"
};
#define n_default_pages (sizeof(default_pages) / sizeof(const char*))

/**** Format of time ****/
#define DATESZ 64
static const char* fmt_time = "%a, %d %b %Y %T %Z";

resp_t* resp_new() {
  resp_t* resp = malloc(sizeof(resp_t));
  resp->hdrs = hdr_new(NULL, NULL);
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
  hdr_reset(resp->hdrs);
}

void resp_free(resp_t* resp) {

  hdr_free(resp->hdrs);

  if (resp->mmbuf)
    mmbuf_free(resp->mmbuf);

  free(resp);
}

static void fill_ctype(char* path, char* ctype) {

  if (caseendswith(path, ".html") ||
      caseendswith(path, ".htm"))
    strcpy0(ctype, "text/html");

  else if (caseendswith(path, ".css"))
    strcpy0(ctype, "text/css");

  else if (caseendswith(path, ".js"))
    strcpy0(ctype, "application/javascript");

  else if (caseendswith(path, "png"))
    strcpy0(ctype, "image/png");

  else if (caseendswith(path, ".jpg") ||
           caseendswith(path, ".jpeg"))
    strcpy0(ctype, "image/jpeg");

  else if (caseendswith(path, ".gif"))
    strcpy0(ctype, "image/gif");

  else
    strcpy0(ctype, "text/plain");
}

// mmap the static file into response.
// fill in st param that's passed in.
// return size of file if success.
//        -1 if error occurs.
static ssize_t resp_mmap(resp_t* resp, const char* path,
                         struct stat* st) {

  int fd;

  if (stat(path, st) < 0) {
#if DEBUG >= 1
    log_line("[resp_mmap] Error in stat path %s", path);
#endif
    return -1;
  }

  if (S_ISDIR(st->st_mode)) {
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

  resp->mmbuf = mmbuf_new(fd, st->st_size);
  if (!resp->mmbuf)
    return -1;

  close(fd);
  return st->st_size;
}

bool resp_build(resp_t* resp, const req_t* req, const char* www) {

  struct stat st;

  // sync Connection field
  resp->alive = req->alive;

  // try several paths
  char path[REQ_URISZ*3];
  strncpy0(path, www, REQ_URISZ);
  strncat(path, req->uri, REQ_URISZ);
#if DEBUG >= 1
  log_line("[recv_to_send] path is %s", path);
#endif

  bool mmapped = resp_mmap(resp, path, &st) >= 0;

  if (!mmapped) {

    char* path_p = path + strlen(path);
    if (path_p[-1] != '/')
      *path_p++ = '/';

    int i;
    for (i = 0; i < n_default_pages; i++) {
      strncpy0(path_p, default_pages[i], REQ_URISZ);
#if DEBUG >= 1
      log_line("[recv_to_send] try path %s", path);
#endif
      mmapped = resp_mmap(resp, path, &st) >= 0;
      if (mmapped)
        break;
    }
  }

  if (!mmapped) {
    resp->status = 404;
    return false;
  }

  /**** update header fields ****/
  hdr_t* hdr;
  resp->clen = st.st_size;

  if (req->method == M_GET || req->method == M_HEAD) {

    hdr = hdr_new("Content-Type", "");
    fill_ctype(path, hdr->val);
    hdr_insert(resp->hdrs, hdr);

    hdr = hdr_new("Last-Modified", "");
    strftime(hdr->val, DATESZ, fmt_time, localtime(&st.st_mtime));
    hdr_insert(resp->hdrs, hdr);
  }

  return st.st_size;
}

ssize_t resp_hdr(const resp_t* resp, char* hdr) {

  char* hdr_p = hdr;
  sprintf(hdr_p, "HTTP/1.1 %s\r\n", resp_title(resp->status));
  hdr_p += strlen(hdr_p);

  char date[DATESZ];
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  strftime(date, DATESZ, fmt_time, &tm);

  sprintf(hdr_p, "Date: %s\r\n", date);
  hdr_p += strlen(hdr_p);

  // TODO: global version
  strcpy0(hdr_p, "Server: Liso/1.0\r\n");
  hdr_p += strlen(hdr_p);

  sprintf(hdr_p, "Connection: %s\r\n", resp->alive ? "keep-alive" : "close");
  hdr_p += strlen(hdr_p);

  sprintf(hdr_p, "Content-Length: %zd\r\n", resp->clen);
  hdr_p += strlen(hdr_p);

  hdr_t* h;
  for (h = resp->hdrs->next; h; h = h->next) {
    sprintf(hdr_p, "%s: %s\r\n", h->key, h->val);
    hdr_p += strlen(hdr_p);
  }

  strcpy0(hdr_p, "\r\n");
  hdr_p += strlen(hdr_p);

  return hdr_p - hdr;
}

const char* resp_title(int code) {
  switch (code) {
    case 200: return title200;
    case 400: return title400;
    case 404: return title404;
    case 411: return title411;
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
    case 411: return msg411;
    case 501: return msg501;
    case 503: return msg503;
    default:
      fprintf(stderr, "Status Code(%d) undefined.\n", code);
      return msg500;
  }
}
