/*
	File:
		util.cpp
	Author:
		Charles MacDonald
	Notes:
		None
*/
#include "shared.h"
using namespace std;

/* Print error message, print ERRNO meaning, and exit program */
void die_errno(char *fmt, ...)
{
	int err = errno;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "%s\n", strerror(err));
	exit(1);
}

/* Print error message and exit program */
void die(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

/* Write socket until all expected data is written */
void write_socket_blocking(int socket_fd, uint8 *buffer, int bytes_to_write, int &bytes_written)
{
	int offset;
	int count;
	
	offset = 0;
	count = bytes_to_write;

	while(offset < bytes_to_write)
	{
		int delta = write(socket_fd, buffer + offset, count);
		
		offset += delta;
		count -= delta;
		
		/* Process status */
		switch(delta)
		{
			case 0: /* EOF; Disconnected */
				die("write_socket_blocking(): Client disconnect.");
				break;
				
			case -1: /* Error */
				die_errno("write_socket_blocking(): ");
				break;
				
			default: /* Data */
				break;
		}
	}

	/* Return the actual count read */
	bytes_written = offset;
}


/* Read socket until all expected data (or more) is read */
void read_socket_blocking(int socket_fd, uint8 *buffer, int bytes_to_read, int &bytes_read)
{
	int offset;
	int count;
	
	offset = 0;
	count = bytes_to_read;

	while(offset < bytes_to_read)
	{
		int delta = read(socket_fd, buffer + offset, count);
		
		offset += delta;
		count -= delta;
		
		/* Process status */
		switch(delta)
		{
			case 0: /* EOF; Disconnected */
				die("read_socket_blocking(): Client disconnect.");
				break;
				
			case -1: /* Error */
				die_errno("read_socket_blocking(): ");
				break;
				
			default: /* Data */
				break;
		}
	}

	/* Return the actual count read */
	bytes_read = offset;
}



/* End */
