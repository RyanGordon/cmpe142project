/*
	File:
		server.cpp
	Author:
		Charles MacDonald
	Notes:
		None
*/

#include "shared.h"
using namespace std;


uint8 *shared_memory = NULL;
int shared_memory_size;



void run_server(char *hostname, int port, int argc, char *argv[])
{

	static int buffer_length = 256;

	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	
	int server_socket_fd;
	int client_socket_fd;
	int status;
	socklen_t socket_length;
	
	// Open server socket
	puts("- Opening server socket");
	server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket_fd == -1)
		die_errno("Error: socket(): ");

	// Bind server socket
	printf("- Binding server socket (hostname=%s, port=%d)\n", hostname, port);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(hostname); /* See INADDR_ANY */
	server_addr.sin_port = htons(port);
	status = bind(
		server_socket_fd, 
		(struct sockaddr *)&server_addr, 
		sizeof(server_addr)
		);
	if(status == -1)
		die_errno("Error: bind(): ");

	// Listen to server socket
	puts("- Listening to server socket");
	listen(server_socket_fd, 5);

#if 0 // get IP address (always 0.0.0.0) when INADDR_ANY used
	char *temp, *result;
	temp = new char [INET_ADDRSTRLEN];
	socket_length  = sizeof(server_addr);
	result = NULL;
	result = (char *)inet_ntop(
		AF_INET,
		(const void *)&server_addr.sin_addr,
		temp,
		socket_length
		);
	if(result == NULL)
		die("inet_ntop():");
	printf("Name: [%s]\n", temp);
#endif

	// Accept connection to server socket
	puts("- Accepting client socket");
	socket_length = sizeof(client_addr);
	client_socket_fd = accept(
		server_socket_fd,
		(struct sockaddr *)&client_addr,
		&socket_length
		);	
	if(client_socket_fd == -1)
		die_errno("Error: accept(): ");
		
	//----------------------------------------------------------------------
	// Allocate shared memory
	
	shared_memory_size = REGION_SIZE;
	shared_memory = new uint8 [shared_memory_size];
	printf("Server: Allocated %08X bytes of network-shared memory.\n", shared_memory_size);
	
	for(int i = 0; i < REGION_SIZE / 0x1000; i++)
	{
		memset(shared_memory + i * 0x1000, 0xA0+i, 0x1000);
	}
	
	//----------------------------------------------------------------------

	uint8 buffer[256];
	uint64 shared_memory_offset;
	int transferred;
	
	bool running = true;
	
	/* Dispatch loop for client commands */
	printf("* Waiting for client commands.\n");	
	while(running)
	{
		/* Read command from client */
		read_socket_blocking(client_socket_fd, buffer, 1, transferred);
	
		switch(buffer[0])
		{
			case 0xA8: /* Request page */

				read_socket_blocking(client_socket_fd, buffer, 8, transferred);

				shared_memory_offset = 0;
				for(int i = 0; i < 8; i++)
					shared_memory_offset |= (uint64)buffer[i] << (i << 3);
			
				printf("* Page request, shared memory offset: %016llX\n", 
					shared_memory_offset);

				write_socket_blocking(
					client_socket_fd, 
					&shared_memory[shared_memory_offset], 
					0x1000, 
					transferred
					);
			
				printf("* Sent %08X of page data to client.\n", transferred);
				break;
			
			case 0xA9: /* Disconnect */
				running = 0;
				break;
			
			default:
				die("ERROR: Server receieved unknown command %02X from client.\n", buffer[0]);
				break;
		}
	}	
	
	
#if 0		
	// Print buffer content
	printf("- Received %d bytes:\n", bytes_read);
	for(int i = 0; i < bytes_read; i++)
	{
		if((i & 0x0F) == 0x00)
			printf("%02X: ", i);
		printf("%02X ", buffer[i]);
		if((i & 0x0F) == 0x0F)
			printf("\n");
	}	
	printf("\n");

	delete []buffer;
#endif	

	delete []shared_memory;

	// Close client socket
	puts("- Closing client socket");
	status = close(client_socket_fd);
	if(status == -1)
		die_errno("Error: close(): client ");

	// Close server socket
	puts("- Closing server socket");
	status = close(server_socket_fd);
	if(status == -1)
		die_errno("Error: close(): server ");

}

/* End */
