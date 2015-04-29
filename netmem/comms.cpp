
#include "shared.h"

/* Wrappers to simplify sending and receieving data */

/* Get multiple bytes */
void comms_get(int client_socket_fd, uint8 *buffer, int length)
{
	int transferred;
	read_socket_blocking(
		client_socket_fd, 
		buffer, 
		length, 
		transferred
		);
}

/* Send multiple bytes */
void comms_send(int client_socket_fd, uint8 *buffer, int length)
{
	int transferred;
	write_socket_blocking(
		client_socket_fd, 
		buffer, 
		length, 
		transferred
		);
}

/* Send byte */
void comms_sendb(int client_socket_fd, uint8 value)
{
	int transferred;
	uint8 buffer[1];
	buffer[0] = value;
	write_socket_blocking(client_socket_fd, buffer, 1, transferred);
}

uint8 comms_getb(int client_socket_fd)
{
	int transferred;
	uint8 buffer[1];
	read_socket_blocking(client_socket_fd, buffer, 1, transferred);
	return buffer[0];
}


/* Send quadword */
void comms_sendq(int client_socket_fd, uint64 value)
{
	uint8 buffer[PTR_SIZE];
	int transferred;
	
	*(uint64 *)&buffer[0] = value;

	write_socket_blocking(client_socket_fd, buffer, PTR_SIZE, transferred);	
}

/* Get 64-bit quantity */
uint64_t comms_getq(int client_socket_fd)
{
	uint8 buffer[PTR_SIZE];
	int transferred;
	read_socket_blocking(client_socket_fd, buffer, PTR_SIZE, transferred);

	return *(uint64 *)&buffer[0];
}


