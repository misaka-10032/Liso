/**
 * @file response.h
 * @brief Provide response related functions.
 * @author Longqi Cai <longqic@andrew.cmu.edu>
 */

#ifndef RESPONSE_H
#define RESPONSE_H

/**
 * @brief Responds error page to client.
 * @param code Status code.
 * @param fd File descriptor to the client.
 */
void resp_err(int code, int fd);

#endif // RESPONSE_H
