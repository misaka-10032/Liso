/**
 * @file request.h
 * @brief This module parses requests.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef REQUEST_H
#define REQUEST_H

#include "buffer.h"
#include "header.h"
#include "utils.h"

#define REQ_URISZ 2048
#define REQ_VERSZ 32
#define REQ_HOSTSZ 256
#define REQ_CTYPESZ 64

/**
 * @brief Parsed request header.
 *
 * Payload is not included, because it may be too large.
 * It will directly be piped to CGI that needs it.
 */
typedef struct {

  // Request line
  enum {
    M_GET=1,
    M_HEAD,
    M_POST,
    M_OTHER
  } method;
  char version[REQ_VERSZ+1];

  // Scheme
  enum {
    HTTP=1,
    HTTPS,
  } scheme;

  char uri[REQ_URISZ+1];
  // Params is pointer in uri
  // They will be separated by \0
  char* params;

  // Commonly used headers
  char host[REQ_HOSTSZ+1];
  ssize_t clen;
  bool alive;

  // All other headers
  hdr_t* hdrs;

  // Remained content length
  ssize_t rsize;
  // Phase of parsing
  enum {
    REQ_START=1,
    REQ_HEADER,
    REQ_BODY,
    REQ_DONE,
    REQ_ABORT,
  } phase;

  // last recved content
  // used for piped requests
  buf_t* last_buf;

  // Type of request
  enum {
    REQ_STATIC=1,
    REQ_DYNAMIC=2,
  } type;
} req_t;

// create an empty request
req_t* req_new();
// reset a request
void req_reset(req_t* req);
// destroy a request
void req_free(req_t* req);
// method in str
const char* req_method(const req_t* req);

/**
 * @brief Parse request header from buffer.
 * @param req The structured header that will be parsed from buf.
 * @param buf The raw buffer to be parsed from.
 * @return Size of buffer that is header.
 *         -status_code if failed to parse.
 *         -400 if format is bad.
 *         -501 method is not supported.
 *         -411 if Content-Length is required but not provided.
 *
 * We assume that the header will be <= BUFSZ. Otherwise,
 * error will be returned.
 */
ssize_t req_parse(req_t* req, buf_t* buf);

/**
 * @brief Pack request into buffer.
 * @param req The structured header that will be parsed from buf.
 * @param buf The raw buffer to be parsed from.
 * @return Size of buffer that is packed.
 *
 * This method assumes req is safe and fits into buf.
 */
ssize_t req_pack(req_t* req, buf_t* buf);

#endif // REQUEST_H
