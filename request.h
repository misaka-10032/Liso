/**
 * @file request.h
 * @brief This module parses requests.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef REQUEST_H
#define REQUEST_H

#include "io.h"

#define HDR_KEYSZ 64
#define HDR_VALSZ 256
#define REQ_URISZ 256
#define REQ_VERSZ 32
#define REQ_HOSTSZ 256
#define REQ_CTYPESZ 64

/**
 * @brief Request headers as key-val pairs.
 *
 * The entire header chain is organized as a singly
 * linked list. The first node is an empty guard.
 */
typedef struct hdr_s {
  char key[HDR_KEYSZ+1];
  char val[HDR_VALSZ+1];
  struct hdr_s* next;
} hdr_t;

// create a new header node
hdr_t* hdr_new(char* key, char* val);
// destroy the entire list of hdrs
void hdr_free(hdr_t* hdrs);

/**
 * @brief Parsed request header.
 *
 * Payload is not included, because it may be too large.
 * It will directly be streamed to CGI that needs it.
 */
typedef struct {
  enum {
    M_GET=1,
    M_HEAD,
    M_POST,
    M_OTHER
  } method;
  char uri[REQ_URISZ+1];
  char version[REQ_VERSZ+1];
  // Commonly used headers
  char host[REQ_HOSTSZ+1];
  char ctype[REQ_CTYPESZ+1];
  ssize_t clen;
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
    REQ_ABORT
  } phase;
} req_t;

// create an empty request
req_t* req_new();
// destroy a request
void req_free(req_t* req);
// insert a header into the header list
void req_insert(req_t* req, hdr_t* hdr);

/**
 * @brief Parse request header from buffer.
 * @param req The structured header that will be parsed from buf.
 * @param buf The raw buffer to be parsed from.
 * @return Size of buffer that is header.
 *         -1 if method is not supported.
 *         -2 if format is bad.
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
