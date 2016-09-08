/**
 * @file errors.h
 * @brief Error handling.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef ERRORS_H
#define ERRORS_H

#include "utils.h"

/**
 * @brief Responds error page to client.
 * @param no Error number.
 * @param fd File descriptor to the client.
 */
void err_resp(int no, int fd);

#endif // ERRORS_H
