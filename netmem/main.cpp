/*
	File:
		main.cpp
	Author:
		Charles MacDonald
	Notes:
		User interface to application
*/

#include "shared.h"
using namespace std;

#define DEFAULT_HOSTNAME	"127.0.0.1"
#define DEFAULT_PORT		6502

enum {
	PGM_UNDEF,	/* Undefined program type */
	PGM_SERVER,	/* Act as server */
	PGM_CLIENT	/* Act as client */
};



int main (int argc, char *argv[])
{
	int pgm_type;
	char hostname[256];
	int port;
	
	pgm_type = PGM_UNDEF;
	port = DEFAULT_PORT;
	strcpy(hostname, DEFAULT_HOSTNAME);
	
	/* Print help if no arguments given */
	if(argc < 2)
	{
		printf("usage %s <s|c> [-p port] [-h hostname]\n", argv[0]);
		printf("Default hostname: %s\n", hostname);
		printf("Default port: %d\n", port);
		return 1;
	}
	
	/* Process program type */
	char user_type = argv[1][0];
	switch(user_type)
	{
		case 'c':
			pgm_type = PGM_CLIENT;
			break;
		case 's':
			pgm_type = PGM_SERVER;
			break;
		default:
			pgm_type = PGM_UNDEF;
			break;
	}
	
	/* Abort if invalid program type was specified */
	if(pgm_type == PGM_UNDEF)
		die("Error: Unknown program type '%c' specified.\n", user_type);
		
	/* Scan for command-line parameters */
	for(int i = 0; i < argc; i++)
	{
		int left = argc - i - 1;
		
		if(strcmp(argv[i], "-p") == 0)
		{
			/* User specified port */
			if(left == 1)
				port = atoi(argv[i+1]);
			else
				die("Error: Insufficient parameters specified.\n");
		}
		else
		if(strcmp(argv[i], "-h") == 0)
		{
			/* User specified IP address */
			if(left == 1)
				strcpy(hostname, argv[i+1]);
			else
				die("Error: Insufficient parameters specified.\n");
		}
	}
	
	/* Validate settings */
	if(port < 0 || port > 65535)
		die("Error: Port must be within 0 to 65535.\n");

	/* Print settings that are being used */
	printf("Program type:   %s\n", pgm_type == PGM_SERVER ? "Server" : "Client");
	printf("Using hostname: %s\n", hostname);
	printf("Using port:     %d\n", port);
	
	/* Run client or server program */
	switch(pgm_type)
	{
		case PGM_SERVER:
			run_server(hostname, port, argc, argv);
			break;
			
		case PGM_CLIENT:
			run_client(hostname, port, argc, argv);
			break;
	}
	
	return 0;
}


/* End */
