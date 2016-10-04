/**
 * @file logging.h
 * @brief Provides logging related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 *
 * Supports mutex for log_line and log_errln.
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
// log raw content
void log_raw(void* data, size_t n);
// flush log
void log_flush();
// stop logging
void log_stop();

#endif // LOGGING_H
