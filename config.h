/**
 * @file config.h
 * @brief TODO
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "Liso/1.0"

typedef struct {
  int http_port;
  int https_port;
  char* log;
  char* lock;
  char* www;
  char* cgi;
  char* prv;
  char* crt;
} conf_t;

#endif // CONFIG_H
