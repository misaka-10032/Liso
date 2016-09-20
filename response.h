/**
 * @file response.h
 * @brief Provide response related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef RESPONSE_H
#define RESPONSE_H

#include "io.h"
#include "utils.h"

#define RESP_FTYPESZ 64
#define RESP_DATESZ 64

typedef struct resp_s {
  enum {
    RESP_READY=1,
    RESP_HEADER,
    RESP_BODY,
    RESP_ABORT,  // decide to respond error, not yet prepared
    RESP_ERROR,  // prepared error, and in the middle of sending error
  } phase;
  int status;
  char ftype[RESP_FTYPESZ+1];
  ssize_t clen;
  bool alive;
  buf_t* mmbuf;
} resp_t;

// constructor
resp_t* resp_new();
// destructor
void resp_free(resp_t* resp);
// reset response
void resp_reset(resp_t* resp);

/**
 * @brief Memory map a static file to resp.
 * @param path The path to the static file.
 * @return Size of file.
 *         -1 if error occurs.
 */
ssize_t resp_mmap(resp_t* resp, char* path);

/**
 * @brief Build header for response.
 * @param resp The response to be built from.
 * @param hdr The header to be built.
 * @return Header size.
 */
ssize_t resp_hdr(const resp_t* resp, char* hdr);

// Returns error title given status code.
const char* resp_title(int code);
// Returns error msg given status code.
const char* resp_msg(int code);

#endif // RESPONSE_H
