#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

void page_request_callback(char *recv_data);

#define MAX(a,b) (((a)>(b))?(a):(b))

#define REQUEST_PAGE 0x80
#define RESPONSE_PAGE_OK 0x81
#define RESPONSE_PAGE_ERR 0x82

#define REQUEST_PAGE_SYNC 0x90
#define RESPONSE_PAGE_SYNC_OK 0x91
#define RESPONSE_PAGE_SYNC_ERR 0x92

static struct cb_id cn_nmmap_id = { CN_NETLINK_USERS + 3, 0x456 };


#define CLIENT_PAGE_SIZE 4096
#define PAGE_OFFSET_SIZE sizeof(uint64_t)

/**
 *
 * Page Request: 1 byte (opcode) | 8 bytes (page offset 64bit number)
 * Page Response: 1 byte (respose code) | client_page_size bytes (page data itself)
 * Sync Request: 1 byte (opcode) | 8 bytes (page offset 64bit number) | client_page_size bytes (page data itself)
 * Sync Response 1 byte (response code)
 *
 */
#define PAGE_REQUEST_SIZE (sizeof(uint8_t) + PAGE_OFFSET_SIZE)
#define PAGE_RESPONSE_SIZE (sizeof(uint8_t) + CLIENT_PAGE_SIZE)
#define SYNC_REQUEST_SIZE (sizeof(uint8_t) + PAGE_OFFSET_SIZE + CLIENT_PAGE_SIZE)
#define SYNC_RESPONSE_SIZE (sizeof(uint8_t))
#define MAX_RECV_SIZE MAX(PAGE_REQUEST_SIZE,SYNC_REQUEST_SIZE)

int sock;
int seq;

static void fill_with_deadbeef(char **ptr, int length) {
        int i;

        *ptr = (char *)calloc(length, sizeof(uint8_t));
        for(i = 0; i < length; i++) {
                (*ptr)[i] = "\xDE\xAD\xBE\xEF"[i&3]; // Charles is da man...
        }
}

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

    m = NLMSG_DATA(nlh);
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

    response_data = (char *)calloc((int)PAGE_RESPONSE_SIZE, sizeof(uint8_t));

    uint64_t request_address = *((uint64_t *)recv_data);
    printf("Recieved request address: %d\n", request_address);

    // TODO: TCP request across network to second server with memory for us
    fill_with_deadbeef(&page, CLIENT_PAGE_SIZE);

    response_data[0] = RESPONSE_PAGE_OK;
    memcpy(&response_data[1], page, CLIENT_PAGE_SIZE);

    msg = (struct cn_msg *)calloc(sizeof(struct cn_msg) + PAGE_RESPONSE_SIZE, sizeof(uint8_t));
    msg->id = cn_nmmap_id;
    msg->len = PAGE_RESPONSE_SIZE;

    memcpy(msg->data, response_data, PAGE_RESPONSE_SIZE);
    netlink_send(msg);
}

int main() {
    bool need_exit = false;
    struct sockaddr_nl l_local;
    struct nlmsghdr *reply;
    struct cn_msg *data;
    char *buf = (char *)calloc((int)MAX_RECV_SIZE, sizeof(uint8_t));
    int len;
    
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

    close(sock);
    return 0;
}
