/*
	File:
		client.cpp
	Author:
		Charles MacDonald
	Notes:
		None
*/

#include "shared.h"
using namespace std;

int client_page_mask;
int client_offs_mask;
int client_page_size;
void *client_region_base;
size_t client_region_size;
struct sigaction action;
int client_socket_fd;

#define CMD_GET_PAGE	0xA8
#define CMD_DISCONNECT	0xA9

void my_sigsegv_handler(int sig, siginfo_t *siginfo, void *ucontext)
{
	uint8 buffer[256];

	uint64 access_address = (uint64)siginfo->si_addr;
	void *page_base = (void *)(access_address & (uint64)client_page_mask);

	/* Enable access to page that caused fault */
	int status = mprotect(
		page_base, 
		client_page_size,	
		PROT_READ | PROT_WRITE
		);
		
#if DEBUG
	printf("DEBUG: Access at %016llX\n", access_address);
	printf("DEBUG: Modified access for %016llX-%016llX at %p\n",
		(uint64)page_base,
		(uint64)page_base+client_page_size-1,
		page_base
		);
#endif		

	/* Verify access was granted */		
	if(status == -1)
	 	die_errno("Handler: mprotect() failed ");
	
	/* Send page request command to server */
	int transferred;
	buffer[0] = CMD_GET_PAGE;
	write_socket_blocking(client_socket_fd, buffer, 1, transferred);

	/* Calculate absolute offset in shared memory area */
	uint64 shared_offs = (uint64)page_base - (uint64)client_region_base;
	
	/* Split 64-bit address into bytes */
	for(int i = 0; i < 8; i++)
		buffer[i] = (shared_offs >> (i << 3)) & 0xFF;

	/* Send address to server */
	write_socket_blocking(client_socket_fd, buffer, 8, transferred);
	
	/* Read page from server over network */
	read_socket_blocking(
		client_socket_fd, 
		(uint8 *)page_base, 
		client_page_size, 
		transferred
		);
}

/*----------------------------------------------------------------------------*/


void run_client(char *hostname, int port, int argc, char *argv[])
{
	uint8 buffer[256];
	int status;
	socklen_t socket_length;
	struct sockaddr_in server_addr;
	struct hostent *server;

	
	/* Open client socket */
	printf("- Status: Opening client socket\n");
	client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(client_socket_fd == -1)
		die_errno("Error: socket(): ");

	/* Get server address from IP string */
	server = gethostbyname(hostname);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, hostname, &server_addr.sin_addr.s_addr);

	printf("- Connecting to server socket (hostname=%s, port=%d)\n", hostname, port);

	/* Establish connection */
	status = connect(
		client_socket_fd, 
		(struct sockaddr *)&server_addr, 
		sizeof(server_addr)
		);
	if(status == -1)
		die_errno("Error: connect(): ");

	//======================================================================
	// We are connected to the server
	//======================================================================

	/* Install SIGSEGV handler */
	action.sa_sigaction = my_sigsegv_handler;
	action.sa_flags = SA_RESTART | SA_SIGINFO;
	sigaction(SIGSEGV, &action, NULL);

	/* Determine page size and mask */	
	client_page_size = getpagesize();
	client_offs_mask = client_page_size - 1;
	client_page_mask = ~client_offs_mask;
	client_region_base = 0;
	client_region_size = REGION_SIZE;

	client_region_base = (char *)mmap(
		NULL, 				// Region base address
		client_region_size, 		// Region length
		PROT_READ|PROT_WRITE,		// Region access flags
		MAP_SHARED|MAP_ANONYMOUS, 	// Region type flags
		-1, 				// File descriptor (not used due to MAP_ANONYMOUS)
		0 				// File offset (see above)
		);
		
	if(client_region_base == MAP_FAILED)
		printf("Error: mmap() failed (%s).\n", strerror(errno));	
	else
		printf("Status: mmap() success\n");

#if DEBUG
	printf("Debug: Client region:\n");
	printf("- Base:      %p\n", client_region_base);
	printf("- Size:      0x%08lX\n", client_region_size);
	printf("- Page size: 0x%08X\n", client_page_size);
	printf("- Base mask: 0x%08X\n", client_page_mask);
	printf("- Offs mask: 0x%08X\n", client_offs_mask);
#endif
	
	/* Disable access to region */
	status = mprotect(
		client_region_base, 
		client_region_size,
		PROT_NONE);
	if(status == -1)
		printf("Error: mprotect() failed (%s).\n", strerror(errno));
	else
		printf("Status: mprotect() success\n");	
		
	//----------------------------------------------------------------------
	// Main program
	//----------------------------------------------------------------------
	/* Carry out action with server and VM manager */

	unsigned char *data = (unsigned char *)client_region_base;
	
	/* Touch some words in the shared memory area to trigger network page mapping */
	for(int i = 0; i < 16; i++)
	{
		printf("** Accessing page %d:\n", i);
		printf("Access data: %02X\n", data[0x000A + i * 0x1000]);
	}

	//----------------------------------------------------------------------
	// Finished
	//----------------------------------------------------------------------

	/* Send disconnect command */
	buffer[0] = CMD_DISCONNECT;
	write(client_socket_fd, buffer, 1);
	
	/* Close client socket */
	puts("- Closing client socket");
	status = close(client_socket_fd);
	if(status == -1)
		die_errno("Error: close(): ");
	
	/* Unmap region */		
	status = munmap(client_region_base, client_region_size);
	if(status == -1)
		printf("Status: munmap() failed (%s).\n", strerror(errno));
	else
		printf("Status: munmap() success\n");
}

/* End */
