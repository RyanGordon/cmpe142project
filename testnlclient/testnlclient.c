#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/connector.h>

#define REQUEST_PAGE 0x80
#define RESPONSE_PAGE_OK 0x81
#define RESPONSE_PAGE_ERR 0x82

#define REQUEST_PAGE_SYNC 0x90
#define RESPONSE_PAGE_SYNC_OK 0x91
#define RESPONSE_PAGE_SYNC_ERR 0x92

static struct cb_id cn_nmmap_id = { CN_NETLINK_USERS + 4, 0x1 };

static const uint32_t client_page_size = 4096;
static const uint32_t page_offset_size = sizeof(uint64_t);

/**
 *
 * Page Request: 1 byte (opcode) | 8 bytes (page offset 64bit number)
 * Page Response: 1 byte (respose code) | client_page_size bytes (page data itself)
 * Sync Request: 1 byte (opcode) | 8 bytes (page offset 64bit number) | client_page_size bytes (page data itself)
 * Sync Response 1 byte (response code)
 *
 */
static const uint32_t page_request_size = sizeof(uint8_t) + page_offset_size;
static const uint32_t page_response_size = sizeof(uint8_t) + client_page_size;
static const uint32_t sync_request_size = sizeof(uint8_t) + page_offset_size + client_page_size;
static const uint32_t sync_response_size = sizeof(uint8_t);
static const uint32_t max_recv_size = max(page_request_size, sync_request_size);

int sock;

void handle_response(cn_msg *msg) {
	uint8_t response_code;
	char *recv_data;

	response_code = msg->data[0];
	switch (response_code) {
		case REQUEST_PAGE:
			recv_data = (char *)calloc(sizeof(uint64_t));
			memcpy(recv_data, msg->data[1], sizeof(uint64_t));
			page_request_callback(recv_data);
			break;
	}
}

void page_request_callback(char *recv_data) {
	struct cn_msg *msg;
	char *response_data;

	response_data = (char *)calloc(page_response_size);

	uint64_t request_address = (uint64_t)recv_data;
	printf("Recieved request address: %d\n", request_address);

	// TODO: TCP request across network to second server with memory for us

	msg = calloc(sizeof(struct cn_msg) + page_response_size);
	msg->id = cn_nmmap_id;
	msg->len = page_response_size;

	memcpy(msg->data, recv_data, page_response_size);

	netlink_send(sock, msg);
}

int main() {
	struct sockaddr_nl l_local;
	struct nlmsghdr *reply;
	struct cn_msg *data;
	char *buf = (char *)calloc(max_recv_size, sizeof(uint8_t));

	sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (s == -1) {
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
		memset(buf, 0, max_recv_size);
		len = recv(sock, buf, max_recv_size, 0);
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