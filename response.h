/**
 * @file response.h
 * @brief Provide response related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef RESPONSE_H
#define RESPONSE_H

#include "io.h"

#define RESP_FTYPESZ 64
#define RESP_DATESZ 64

typedef struct resp_s {
  enum {
    RESP_READY=1,
    RESP_START,
    RESP_DONE,
    RESP_ABORT
  } phase;
  char ftype[RESP_FTYPESZ+1];
  buf_t* mmbuf;
} resp_t;

// constructor
resp_t* resp_new();
// destructor
void resp_free(resp_t* resp);

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
size_t resp_hdr(const resp_t* resp, char* hdr);

/**
 * @brief Responds error page to client.
 * @param code Status code.
 * @param fd File descriptor to the client.
 */
void resp_err(int code, int fd);

#endif // RESPONSE_H
