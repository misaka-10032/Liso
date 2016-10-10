/**
 * @file header.h
 * @brief HTTP header struct, organized as singly linked list.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef HEADER_H
#define HEADER_H

#define HDR_KEYSZ 512
#define HDR_VALSZ 4096

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
// insert a header into the header list. NOT copying.
void hdr_insert(hdr_t* hdrs, hdr_t* hdr);
// reset a list of headers
void hdr_reset(hdr_t* hdrs);

#endif // HEADER_H
