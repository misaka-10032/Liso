/**
 * @file logging.h
 * @brief Provides logging related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 * @bug This module is NOT thread safe.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include "io.h"
#include "utils.h"

#define LINESZ BUFSZ

// init logging
void log_init(char* fname);
// check if logging is inited
bool log_inited();
// log a new line
void log_line(char* fmt, ...);
// log an error line
void log_errln(char* fmt, ...);
// stop logging
void log_stop();

#endif // LOGGING_H
