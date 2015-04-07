#ifndef _UTIL_H_
#define _UTIL_H_

/* Function prototypes */
void die_errno(char *fmt, ...);
void die(char *fmt, ...);
void read_socket_blocking(int socket_fd, uint8 *buffer, int bytes_to_read, int &bytes_read);
void write_socket_blocking(int socket_fd, uint8 *buffer, int bytes_to_write, int &bytes_written);

#endif /* _UTIL_H_ */

