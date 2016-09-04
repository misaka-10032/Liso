/******************************************************************************
* utils.h                                                                     *
*                                                                             *
* Description: Utility functions.                                             *
*                                                                             *
* Author(s):   Longqi Cai (longqic@andrew.cmu.edu)                            *
*                                                                             *
*******************************************************************************/

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 1

#ifndef max
  #define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
  #define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define CRLF "\r\n"

#endif // UTILS_H
