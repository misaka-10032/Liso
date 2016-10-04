/**
 * @file response.h
 * @brief Provide response related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef RESPONSE_H
#define RESPONSE_H

#include "buffer.h"
#include "request.h"
#include "header.h"
#include "utils.h"
#include "config.h"

typedef struct resp_s {
  enum {
    RESP_READY=1,
    RESP_HEADER,
    RESP_BODY,
    RESP_ABORT,  // decide to respond error, not yet prepared
    RESP_ERROR,  // prepared error, and in the middle of sending error
    RESP_DISABLED,  // to serve cgi
  } phase;
  int status;
  ssize_t clen;
  bool alive;
  hdr_t* hdrs;
  buf_t* mmbuf;
} resp_t;

// constructor
resp_t* resp_new();
// destructor
void resp_free(resp_t* resp);
// reset response
void resp_reset(resp_t* resp);

/**
 * @brief Build response from request.
 * @param resp The response to build.
 * @param req The request to be served.
 * @param conf The global configuration.
 * @return  true if normal.
 *         false if error occurs.
 *
 * Some fields in resp will be updated.
 */
bool resp_build(resp_t* resp, const req_t* req, const conf_t* conf);

/**
 * @brief Serialize header for response.
 * @param resp The response to be built from.
 * @param hdr The header to be built.
 * @return Serialized header size.
 */
ssize_t resp_hdr(const resp_t* resp, char* hdr);

// Returns error title given status code.
const char* resp_title(int code);
// Returns error msg given status code.
const char* resp_msg(int code);

#endif // RESPONSE_H
