/*
    File:
        client.cpp
    Author:
        Charles MacDonald
        Ryan Gordon
        Britto Thomas
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

#define DEFAULT_CLIENT_PAGE_SIZE    0x1000
#define DEFAULT_CLIENT_MEMORY_SIZE  0x10000

void page_request_callback(uint64_t page_offset);
void page_sync_request_callback(uint64_t page_offset, char *page);
bool nm_client_request_page(int client_socket_fd, uint64_t value, uint8_t *buffer);
bool nm_client_request_sync(int client_socket_fd, uint64_t value, uint8_t *buffer);

static struct cb_id cn_nmmap_id = { CN_NETLINK_USERS + 3, 0x456 };

/* Netlink stuff */

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
    char *recv_data2;

    response_code = msg->data[0];
    switch (response_code) {
        case REQUEST_PAGE:
            recv_data = (char *)calloc(PAGE_OFFSET_SIZE, sizeof(uint8_t));
            memcpy(recv_data, &msg->data[1], PAGE_OFFSET_SIZE);
            page_request_callback(*((uint64_t *)recv_data));
            break;
        case REQUEST_PAGE_SYNC:
            recv_data = (char *)calloc(PAGE_OFFSET_SIZE, sizeof(uint8_t));
            memcpy(recv_data, &msg->data[1], PAGE_OFFSET_SIZE);
            recv_data2 = (char *)calloc(CLIENT_PAGE_SIZE, sizeof(uint8_t));
            memcpy(recv_data2, &msg->data[1 + PAGE_OFFSET_SIZE], CLIENT_PAGE_SIZE);
            page_sync_request_callback(*((uint64_t *)recv_data), recv_data2);
    }
}

void page_sync_request_callback(uint64_t page_offset, char *page) {
    struct cn_msg *msg;
    char *response_data;
    bool ret;

    response_data = (char *)calloc((int)SYNC_RESPONSE_SIZE, sizeof(uint8_t));

    printf("Synchronizing page: %016llX\n", page_offset);
    ret = nm_client_request_sync(client_socket_fd, page_offset, (uint8_t *)page);

    msg = (struct cn_msg *)calloc(sizeof(struct cn_msg) + SYNC_RESPONSE_SIZE, sizeof(uint8_t));
    msg->id = cn_nmmap_id;
    msg->len = SYNC_RESPONSE_SIZE;

    if (ret) {
        *response_data = RESPONSE_PAGE_SYNC_OK;
    } else {
        *response_data = RESPONSE_PAGE_SYNC_ERR;
    }

    memcpy(msg->data, response_data, SYNC_RESPONSE_SIZE);
    netlink_send(msg);
}

void page_request_callback(uint64_t page_offset) {
    struct cn_msg *msg;
    char *response_data;
    char *page;

    response_data = (char *)calloc((int)PAGE_RESPONSE_SIZE, sizeof(uint8_t));
    page = (char *)calloc((int)CLIENT_PAGE_SIZE, sizeof(uint8_t));

    printf("Recieved request address: %016llX\n", page_offset);
    nm_client_request_page(client_socket_fd, page_offset, (uint8_t *)page);
    response_data[0] = RESPONSE_PAGE_OK;
    memcpy(&response_data[1], page, CLIENT_PAGE_SIZE);

    msg = (struct cn_msg *)calloc(sizeof(struct cn_msg) + PAGE_RESPONSE_SIZE, sizeof(uint8_t));
    msg->id = cn_nmmap_id;
    msg->len = PAGE_RESPONSE_SIZE;

    memcpy(msg->data, response_data, PAGE_RESPONSE_SIZE);
    netlink_send(msg);
}

/* Client-side functions */

bool nm_client_connect(uint64_t page_size, uint64_t memory_size) {
    /* Send command and parameters */
    comms_sendb(client_socket_fd, CLIENT_CONNECT);
    comms_sendq(client_socket_fd, page_size);
    comms_sendq(client_socket_fd, memory_size);

    /* Get status */
    uint8_t status = comms_getb(client_socket_fd);

    /* Return status */
    return (status == NM_RESPONSE_ACK) ? true : false;
}

void nm_client_disconnect(void) {
    comms_sendb(client_socket_fd, CLIENT_DISCONNECT);
}

bool nm_client_request_sync(int client_socket_fd, uint64_t value, uint8_t *buffer) {
    /* Send page request command to server */
    comms_sendb(client_socket_fd, REQUEST_PAGE_SYNC);

    /* Send offset */
    comms_sendq(client_socket_fd, value);

    /* Read page from server over network */
    comms_send(client_socket_fd, buffer, client_page_size);

    return true;
}

bool nm_client_request_page(int client_socket_fd, uint64_t value, uint8_t *buffer) {
    uint8 status = NM_RESPONSE_ACK;

    /* Send page request command to server */
    comms_sendb(client_socket_fd, REQUEST_PAGE);

    /* Send offset */
    comms_sendq(client_socket_fd, value);

    /* Send page if status is OK */
    comms_get(client_socket_fd, buffer, client_page_size);

    return true;
}


int run_client(char *hostname, int port, int argc, char *argv[]) {
    int status;
    int len;
    struct sockaddr_in server_addr;
    struct hostent *server;
    struct sockaddr_nl l_local;
    struct nlmsghdr *reply;
    struct cn_msg *data;
    char *buf = (char *)calloc((int)MAX_RECV_SIZE, sizeof(uint8_t));
    bool running = true;

    seq = 0;

    /* Open client socket */
    printf("- Status: Opening client socket\n");
    client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket_fd == -1) {
        die_errno("Error: socket(): ");
    }

    /* Get server address from IP string */
    server = gethostbyname(hostname);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, hostname, &server_addr.sin_addr.s_addr);
    printf("- Connecting to server socket (hostname=%s, port=%d)\n", hostname, port);

    /* Establish connection */
    status = connect(client_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (status == -1) {
        die_errno("Error: connect(): ");
    }


    if (!nm_client_connect(DEFAULT_CLIENT_PAGE_SIZE, DEFAULT_CLIENT_MEMORY_SIZE)) {
        printf("Error: nm_client_connect():\n");
        return -1;
    }

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

    //======================================================================
    // We are now connected to the server and the kernel netlink
    //======================================================================

    while (running) {
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

    //----------------------------------------------------------------------
    // Finished
    //----------------------------------------------------------------------

    /* Send disconnect command */
    nm_client_disconnect();

    /* Close client socket */
    puts("- Closing client socket");
    status = close(client_socket_fd);
    if (status == -1) {
        die_errno("Error: close(): ");
    }

    close(sock);
    return 0;
}
