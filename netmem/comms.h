#ifndef _COMMS_H_
#define _COMMS_H_

/* Function prototypes */
void comms_get(int client_socket_fd, uint8 *buffer, int length);
void comms_send(int client_socket_fd, uint8 *buffer, int length);

void comms_sendb(int client_socket_fd, uint8 value);
uint8 comms_getb(int client_socket_fd);

void comms_sendq(int client_socket_fd, uint64 value);
uint64_t comms_getq(int client_socket_fd);


#endif /* _UTIL_H_ */

