/**
 * Kernel Module that registers a function to 
 * with the kernel to catch all network_mmap 
 * page faults. This allows us to do the 
 * network handling inside this kernel module
 * and iterate quickly rather then inside the
 * kernel itself and have to recompile a new
 * kernel each time, which is slow and hard
 * to debug
 * 
 * To compile type: make
 * To load into kernel: insmod ./nmmapmod.ko
 * To unload from kernel: rmmod nmmapmod
 **/

#include <linux/module.h>       // Needed by all modules
#include <linux/kernel.h>       // Needed for KERN_INFO
#include <linux/init.h>         // Needed for the macros
#include <linux/mm.h>           // Needed for vm_fault and vm_area_struct
#include <linux/skbuff.h>       // Needed for netlink
#include <linux/connector.h>    // Needed for netlink

static void page_recv_callback(char *page_recieved);

#define DRIVER_AUTHOR "Ryan Gordon <rygorde4@gmail.com>, Charles MacDonald <chamacd@gmail.com>"
#define DRIVER_DESC   "Networked mmap page fault handler"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

#define REQUEST_PAGE 0x80
#define RESPONSE_PAGE_OK 0x81
#define RESPONSE_PAGE_ERR 0x82

#define REQUEST_PAGE_SYNC 0x90
#define RESPONSE_PAGE_SYNC_OK 0x91
#define RESPONSE_PAGE_SYNC_ERR 0x92

#define CLIENT_PAGE_SIZE 4096
#define PAGE_OFFSET_SIZE sizeof(uint64_t)

/**
 *
 * Page Request: 1 byte (opcode) | 8 bytes (page offset 64bit number)
 * Page Response: 1 byte (respose code) | CLIENT_PAGE_SIZE bytes (page data itself)
 * Sync Request: 1 byte (opcode) | 8 bytes (page offset 64bit number) | CLIENT_PAGE_SIZE bytes (page data itself)
 * Sync Response 1 byte (response code)
 *
 */
#define PAGE_REQUEST_SIZE sizeof(uint8_t) + PAGE_OFFSET_SIZE
#define PAGE_RESPONSE_SIZE sizeof(uint8_t) + CLIENT_PAGE_SIZE
#define SYNC_REQUEST_SIZE sizeof(uint8_t) + PAGE_OFFSET_SIZE + CLIENT_PAGE_SIZE
#define SYNC_RESPONSE_SIZE sizeof(uint8_t)

static struct cb_id cn_nmmap_id = { CN_NETLINK_USERS + 4, 0x1 };
static char cn_nmmap_name[] = "cn_nmmap_msg";

bool g_response_recieved = false;
char *g_response_data = NULL;

static void cn_nmmap_msg_callback(struct cn_msg *msg, struct netlink_skb_parms *nsp) {
        uint8_t response_code;
        char *response_data;
        int i;

        pr_info("%s: %lu: idx=%x, val=%x, seq=%u, ack=%u, len=%d: %s.\n",
                __func__, jiffies, msg->id.idx, msg->id.val,
                msg->seq, msg->ack, msg->len,
                msg->len ? (char *)msg->data : "");

        response_code = msg->data[0];
        switch (response_code) {
                case RESPONSE_PAGE_OK:
                        response_data = kzalloc(CLIENT_PAGE_SIZE, GFP_ATOMIC);
                        memcpy(response_data, &msg->data[1], CLIENT_PAGE_SIZE);
                        page_recv_callback(response_data);
                        break;
                case RESPONSE_PAGE_ERR:
                        response_data = kzalloc(CLIENT_PAGE_SIZE, GFP_ATOMIC);
                        for(i = 0; i < CLIENT_PAGE_SIZE; i++) {
                                response_data[i] = "\xDE\xAD\xBE\xEF"[i&3]; // Charles is da man...
                        }
                        page_recv_callback(response_data);
                        break;
        }
}

static int cn_nmmap_send_msg(char *data, uint32_t length) {
        struct cn_msg *send_msg;

        send_msg = kzalloc(sizeof(struct cn_msg) + length, GFP_ATOMIC);
        if (!send_msg) {
                return 1;
        }
        send_msg->id = cn_nmmap_id;
        send_msg->len = length;

        memcpy(send_msg->data, data, length);

        cn_netlink_send(send_msg, 0, GFP_ATOMIC);
        kfree(send_msg);

        return 0;
}

static void page_recv_callback(char *page_recieved) {
        g_response_data = page_recieved;
        g_response_recieved = true;
}

static void wait_for_response() {
        while (g_response_recieved == false) msleep(1);
        g_response_recieved = false;
}

static int network_mmap_fault_module_handler(struct vm_area_struct *vma, struct vm_fault *vmf) {
        char *virt_page;
        struct page *page;
        char *nmmap_send_msg;
        uint64_t faulted_page;

        // Get the faulted page number - TODO: Is this right??
        faulted_page = vmf->pgoff << PAGE_SHIFT;

        printk(KERN_INFO "network_mmap_fault_module_handler: Called pgoff: %d\n", faulted_page);

        // Prepare the network request
        nmmap_send_msg = kzalloc(PAGE_REQUEST_SIZE, GFP_ATOMIC);
        nmmap_send_msg[0] = REQUEST_PAGE;
        *((uint64_t *)&nmmap_send_msg[1]) = faulted_page;

        // Send the request away
        cn_nmmap_send_msg(nmmap_send_msg, PAGE_REQUEST_SIZE);
        wait_for_response(); // Wait for the response

        // Create's a page and fills it with the data recieved from over the network
        virt_page = (char *)get_zeroed_page(GFP_USER);
        memcpy(virt_page, g_response_data, CLIENT_PAGE_SIZE);

        page = virt_to_page(virt_page);
        get_page(page); // Increments reference count of page
        vmf->page = page;
        printk(KERN_INFO "virt_page: %016llX, page: %016llX, vmf->page: %016llX\n", (char)virt_page, (char)page, (char)vmf->page);

        return 0; // If a page isn't being loaded then you need to return VM_SIGFAULT_BUS otherwise the kernel itself will crash
}

static int __init nmmapmod_init(void) {
        int err;

        printk(KERN_INFO "Loading Network MMAP Kernel Module with page fault handler\n");

        err = cn_add_callback(&cn_nmmap_id, cn_nmmap_name, cn_nmmap_msg_callback);
        if (err) {
                cn_del_callback(&cn_nmmap_id);
                return 1;
        }

        set_kmod_network_mmap_fault_handler(network_mmap_fault_module_handler);
        return 0;
}

static void __exit nmmapmod_exit(void) {
        set_kmod_network_mmap_fault_handler(NULL);
        cn_del_callback(&cn_nmmap_id);
        printk(KERN_INFO "Unloaded Network MMAP Kernel Module\n");
}

module_init(nmmapmod_init);
module_exit(nmmapmod_exit);
