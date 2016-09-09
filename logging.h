/**
 * @file logging.h
 * @brief Provides logging related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef LOGGING_H
#define LOGGING_H

#define LINESZ 1024
#define DTSZ 80

// init logging
void log_init(char* fname);
// log a new line
void log_line(char* fmt, ...);
// stop logging
void log_stop();

#endif // LOGGING_H
