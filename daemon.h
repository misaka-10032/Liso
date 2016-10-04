/**
 * @file daemon.h
 * @brief Daemonize current process.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef DAEMON_H
#define DAEMON_H

/**
 * @brief Daemonize current process, setting up signal handlers.
 * @param lock_file Lock file to prevent multiple instance.
 *
 * This function may not return if error occurs.
 */
void daemonize(char* lock_file);

#endif // DAEMON_H
