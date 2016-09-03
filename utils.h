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

#define DEBUG 1

#ifndef max
  #define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
  #define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#endif // UTILS_H
