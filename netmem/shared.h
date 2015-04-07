#ifndef _SHARED_H_
#define _SHARED_H_

#define DEBUG	1

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned long int uint32;
typedef unsigned long long uint64;

#define REGION_SIZE	0x10000

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.h"
#include "server.h"
#include "client.h"

#endif /* _SHARED_H_ */
