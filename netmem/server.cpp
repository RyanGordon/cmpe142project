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

void save_write(void);

/*------------------------------------------------*/

/* This is the network memory */
uint8 *shared_memory = NULL;
static int shared_memory_size = 0x10000; 	/* Fixed: 64K */
static int shared_page_size = 0x1000;	/* Fixed: 4K */

void command_request_page_sync(int client_socket_fd)
{
	uint64 shared_memory_offset;
		
	/* Get offset of page from client */
	shared_memory_offset = comms_getq(client_socket_fd);

	/* Debug */
	printf("* Page sync request, shared memory offset: %016llX\n", 
		shared_memory_offset);

	/* Read memory */
	comms_get(
		client_socket_fd, 
		&shared_memory[shared_memory_offset], 
		shared_page_size 
		);

	save_write();
}

void command_request_page(int client_socket_fd)
{
	uint64 shared_memory_offset;
	int transferred;
		
	/* Get offset of page from client */
	shared_memory_offset = comms_getq(client_socket_fd);

	/* Debug */
	printf("* Page data request, shared memory offset: %016llX\n", 
		shared_memory_offset);

	/* Write memory*/
	comms_send(
		client_socket_fd, 
		&shared_memory[shared_memory_offset], 
		shared_page_size
		);
}

/*
	Client sends
	byte  - opcode
	qword - local page size
	qword - shared memory size
	Server responds with
	ACK - Page size and memory size accepted
	NACK - Connection denied (invalid page size or memory size)
*/
bool command_connect(int client_socket_fd)
{
	bool error = false;
	
	uint64 page_size = comms_getq(client_socket_fd);
	uint64 memory_size = comms_getq(client_socket_fd);
	
	printf("Client connect: page_size=%016llX, memory_size=%016llx\n",
		page_size, memory_size);
		
	if(page_size != 0x1000)
		error = true;
	if(memory_size > 0x10000)
		error = true;
		
	/* If default memory exists, deallocate */
	if(shared_memory) delete []shared_memory;
	
	/* Reallocate new memory */
	shared_memory = new uint8 [memory_size];
	if(!shared_memory)
		error = true;
		
	/* Update globals */
	shared_page_size = page_size;
	shared_memory_size = memory_size;

	comms_sendb(client_socket_fd, error ? NM_RESPONSE_NACK : NM_RESPONSE_ACK);	
	
	return error;
}

void command_disconnect(int client_socket_fd)
{
	/* */
}

void save_write(void) {
	/* Save (possibly synchronized) shared memory) */
	FILE *fd;
	fd = fopen("shared.bin", "wb");
	fwrite(shared_memory, shared_memory_size, 1, fd);
	fclose(fd);
}

void server_dispatch_command(int client_socket_fd)
{
	bool running = true;
	
	/* Dispatch loop for client commands */
	printf("* Waiting for client commands.\n");	
	while(running)
	{
		uint8 opcode = comms_getb(client_socket_fd);
	
		switch(opcode)
		{
			case REQUEST_PAGE_SYNC: /* Request page sync */
				command_request_page_sync(client_socket_fd);
				break;
				
			case REQUEST_PAGE: /* Request page data */
				command_request_page(client_socket_fd);
				break;
				
			case CLIENT_CONNECT: /* Client protocol connect to server */
				if(command_connect(client_socket_fd))
					return;
				break;
			
			case CLIENT_DISCONNECT: /* Client protocol disconnect from server */
				command_disconnect(client_socket_fd);
							
				/* Client is disconnecting so we will disconnect too */
				running = 0;
				break;
			
			default: /* Unknown instruction */
				die("ERROR: Server receieved unknown command %02X from client.\n", 
					opcode);
				break;
		}
	}
}

/*------------------------------------------------*/

void run_server(char *hostname, int port, int argc, char *argv[])
{

	static int buffer_length = 256;

	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	
	int server_socket_fd;
	int client_socket_fd;
	int status;
	int sockoptval = 1;
	socklen_t socket_length;
	
	// Open server socket
	puts("- Opening server socket");
	server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket_fd == -1)
		die_errno("Error: socket(): ");

	if (setsockopt(server_socket_fd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *) &sockoptval, sizeof(sockoptval)) < 0) {
		die_errno("Could not set REUSEPORT | REUSEADDR");
	}

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
	
	shared_memory = new uint8 [shared_memory_size];
	memset(shared_memory, 0x20, shared_memory_size);
	strcpy((char *)shared_memory, "HELLO THIS IS US. WE ARE SPARTA: RYAN, CHARLES, BRITTO, ANDREW, EDWIN\n\x00");

	printf("Server: Allocated %08X bytes of network-shared memory.\n", shared_memory_size);
	
	//----------------------------------------------------------------------

	/* Run dispatch until quit requested by client */
	server_dispatch_command(client_socket_fd);
	
	printf("\n***Server dispatch loop exit.\n");

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
