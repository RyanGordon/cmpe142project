#ifndef _SHARED_H_
#define _SHARED_H_

#define DEBUG	1

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned long int uint32;
typedef unsigned long long uint64;


// NEW CODE START

#define PTR_SIZE	8

#define REQUEST_PAGE 		0x80
#define RESPONSE_PAGE_OK 	0x81
#define RESPONSE_PAGE_ERR 	0x82

#define REQUEST_PAGE_SYNC 	0x90
#define RESPONSE_PAGE_SYNC_OK 	0x91
#define RESPONSE_PAGE_SYNC_ERR 	0x92

#define RESPONSE_PAGE_ALL_SYNC	0x70 /* sync all pages */

#define CLIENT_CONNECT		0xA0 /* op:1, pagesize:4, memorysize:4 */

#define CLIENT_DISCONNECT	0xB0 /* op:1 */

#define NM_RESPONSE_ACK		0xE0
#define NM_RESPONSE_NACK	0xF0

#define CLIENT_PAGE_SIZE 4096
#define PAGE_OFFSET_SIZE sizeof(uint64_t)

#define PAGE_REQUEST_SIZE sizeof(uint8_t) + PAGE_OFFSET_SIZE
#define PAGE_RESPONSE_SIZE sizeof(uint8_t) + CLIENT_PAGE_SIZE
#define SYNC_REQUEST_SIZE sizeof(uint8_t) + PAGE_OFFSET_SIZE + CLIENT_PAGE_SIZE
#define SYNC_RESPONSE_SIZE sizeof(uint8_t)
#define MAX_RECV_SIZE max(PAGE_REQUEST_SIZE,SYNC_REQUEST_SIZE)

// NEW CODE END

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "util.h"
#include "comms.h"
#include "server.h"
#include "client.h"
#include <algorithm>


#endif /* _SHARED_H_ */
