/**
 * @file log.h
 * @brief Provides logging related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef LOG_H
#define LOG_H

#define LINESZ 1024
#define DTSZ 80

// init logging
void log_init(char* fname);
// log a new line
void log_line(char* fmt, ...);
// stop logging
void log_stop();

#endif // LOG_H
