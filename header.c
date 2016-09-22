/**
 * @file header.c
 * @brief Implementation of header.h
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#include "header.h"
#include "utils.h"

hdr_t* hdr_new(char* key, char* val) {
  hdr_t* hdr = malloc(sizeof(hdr_t));
  if (key)
    strncpy0(hdr->key, key, HDR_KEYSZ);
  else
    hdr->key[0] = 0;
  if (val)
    strncpy0(hdr->val, val, HDR_VALSZ);
  else
    hdr->val[0] = 0;
  hdr->next = NULL;
  return hdr;
}

void hdr_free(hdr_t* hdrs) {
  if (!hdrs)
    return;

  hdr_t* p = hdrs;
  while (p) {
    hdr_t* q = p->next;
    free(p);
    p = q;
  }
}

void hdr_insert(hdr_t* hdrs, hdr_t* hdr) {
  hdr->next = hdrs->next;
  hdrs->next = hdr;
}

void hdr_reset(hdr_t* hdrs) {
  hdr_free(hdrs->next);
  hdrs->next = NULL;
}
