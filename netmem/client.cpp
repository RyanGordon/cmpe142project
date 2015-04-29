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
int sock;
int seq;

#define DEFAULT_CLIENT_PAGE_SIZE	0x1000
#define DEFAULT_CLIENT_MEMORY_SIZE	0x10000

void page_request_callback(char *recv_data);
bool nm_client_request_page(int, uint64_t, uint8_t *);

static struct cb_id cn_nmmap_id = { CN_NETLINK_USERS + 3, 0x456 };

static int netlink_send(struct cn_msg *msg) {    
    struct nlmsghdr *nlh;
    unsigned int cn_msg_size;
    unsigned int total_size;
    int err;
    char *buf;
    struct cn_msg *m;

    cn_msg_size = sizeof(struct cn_msg) + msg->len;
    total_size = NLMSG_SPACE(cn_msg_size);
    buf = (char *)calloc(total_size, sizeof(uint8_t));
    
    nlh = (struct nlmsghdr *)buf;
    nlh->nlmsg_seq = seq++;
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_type = NLMSG_DONE;
    nlh->nlmsg_len = total_size;
    nlh->nlmsg_flags = 0;

    m = (struct cn_msg *)NLMSG_DATA(nlh);
    memcpy(m, msg, cn_msg_size);
    
    printf("Sending response with size: %d, %d\n", nlh->nlmsg_len, cn_msg_size);
    err = send(sock, nlh, total_size, 0);
    printf("send response code: %d\n", err);
    if (err == -1) {
        printf("Failed to send: %s [%d].\n", strerror(errno), errno);
    }

    return err;
}

void handle_response(struct cn_msg *msg) {
    uint8_t response_code;
    char *recv_data;

    response_code = msg->data[0];
    switch (response_code) {
        case REQUEST_PAGE:
            recv_data = (char *)calloc(PAGE_OFFSET_SIZE, sizeof(uint8_t));
            memcpy(recv_data, &msg->data[1], PAGE_OFFSET_SIZE);
            page_request_callback(recv_data);
            break;
    }
}

void page_request_callback(char *recv_data) {
    struct cn_msg *msg;
    char *response_data;
    char *page;
    uint64_t request_address;

    response_data = (char *)calloc((int)PAGE_RESPONSE_SIZE, sizeof(uint8_t));
    page = (char *)calloc((int)CLIENT_PAGE_SIZE,sizeof(uint8_t));
    request_address = *((uint64_t *)recv_data);
    printf("Recieved request address: %d\n", request_address);
    nm_client_request_page(client_socket_fd, request_address, (uint8_t *) page);	
    response_data[0] = RESPONSE_PAGE_OK;
    memcpy(&response_data[1], page, CLIENT_PAGE_SIZE);

    msg = (struct cn_msg *)calloc(sizeof(struct cn_msg) + PAGE_RESPONSE_SIZE, sizeof(uint8_t));
    msg->id = cn_nmmap_id;
    msg->len = PAGE_RESPONSE_SIZE;

    memcpy(msg->data, response_data, PAGE_RESPONSE_SIZE);
    netlink_send(msg);
}

/* Client-side functions */

bool nm_client_connect(uint64_t page_size, uint64_t memory_size)
{
	/* Send command and parameters */
	comms_sendb(client_socket_fd, CLIENT_CONNECT);
	comms_sendq(client_socket_fd, page_size);
	comms_sendq(client_socket_fd, memory_size);
	
	/* Get status */
	uint8_t status = comms_getb(client_socket_fd);

	/* Return status */	
	return (status == NM_RESPONSE_ACK) ? true : false;
}

void nm_client_disconnect(void)
{
	comms_sendb(client_socket_fd, CLIENT_DISCONNECT);
}

bool nm_client_request_sync(int client_socket_fd, uint64_t value, uint8_t *buffer)
{
	/* Send page request command to server */
	comms_sendb(client_socket_fd, REQUEST_PAGE_SYNC);
	
	/* Send offset */
	comms_sendq(client_socket_fd, value);
	
	/* Read page from server over network */
	comms_send(client_socket_fd, buffer, client_page_size);

	return true;
}

bool nm_client_request_page(int client_socket_fd, uint64_t value, uint8_t *buffer)
{
	uint8 status = NM_RESPONSE_ACK;
	
	/* Send page request command to server */
	comms_sendb(client_socket_fd, REQUEST_PAGE);

	/* Send offset */
	comms_sendq(client_socket_fd, value);
	
	/* Send page if status is OK */
	comms_get(client_socket_fd, buffer, client_page_size);

	return true;
}


int run_client(char *hostname, int port, int argc, char *argv[])
{
	int status;
	struct sockaddr_in server_addr;
	struct hostent *server;
	bool need_exit = false;
    	struct sockaddr_nl l_local;
    	struct nlmsghdr *reply;
    	struct cn_msg *data;
    	char *buf = (char *)calloc((int)MAX_RECV_SIZE, sizeof(uint8_t));
    	int len;

	
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


	if(!nm_client_connect(DEFAULT_CLIENT_PAGE_SIZE, DEFAULT_CLIENT_MEMORY_SIZE))
	{
		printf("Error: nm_client_connect():\n");
		return -1;
	}

    seq = 0;

    sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    l_local.nl_family = AF_NETLINK;
    l_local.nl_groups = -1; // bitmask of requested groups
    l_local.nl_pid = 0;

    if (bind(sock, (struct sockaddr *)&l_local, sizeof(struct sockaddr_nl)) == -1) {
        perror("bind");
        close(sock);
        return -1;
    }

    while (!need_exit) {
        memset(buf, 0, MAX_RECV_SIZE);
        len = recv(sock, buf, MAX_RECV_SIZE, 0);
        if (len == -1) {
            perror("recv buf");
            close(sock);
            return -1;
        }

        reply = (struct nlmsghdr *)buf;
        switch (reply->nlmsg_type) {
            case NLMSG_ERROR:
                printf("Error message received.\n");
                break;
            case NLMSG_DONE:
                data = (struct cn_msg *)NLMSG_DATA(reply);
                handle_response(data);
                break;
            default:
                break;
        }
    }

	//======================================================================
	// We are connected to the server
	//======================================================================

	/*
	for(int i = 0; i < client_region_size/client_page_size; i++)	
	{
		printf("synchronizing page %d\n", i);
		nm_client_request_sync(
			client_socket_fd, 
			i*client_page_size, 
			&data[i*client_page_size]
			);
	}
	*/
	//----------------------------------------------------------------------
	// Finished
	//----------------------------------------------------------------------

	/* Send disconnect command */
	nm_client_disconnect();
	
	/* Close client socket */
	puts("- Closing client socket");
	status = close(client_socket_fd);
	if(status == -1)
		die_errno("Error: close(): ");
	
	close(sock);
	return 0;
}

/* End */
