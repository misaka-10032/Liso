/**
 * @file logging.h
 * @brief Provides logging related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 * @bug This module is NOT thread safe.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include "io.h"

#define LINESZ BUFSZ

// init logging
void log_init(char* fname);
// log a new line
void log_line(char* fmt, ...);
// stop logging
void log_stop();

#endif // LOGGING_H
