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
static void page_sync_recv_callback(char *page_sync_resp_code);

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

static struct cb_id cn_nmmap_id = { CN_NETLINK_USERS + 3, 0x456 };
static char cn_nmmap_name[] = "cn_nmmap_msg";

bool g_response_recieved = false;
char *g_response_data = NULL;

static void fill_with_deadbeef(char **ptr, int length) {
        int i;

        *ptr = kzalloc(length, GFP_ATOMIC);
        for(i = 0; i < length; i++) {
                (*ptr)[i] = "\xDE\xAD\xBE\xEF"[i&3]; // (Such_skill *much_respect)->Charles
        }
}

static void cn_nmmap_msg_callback(struct cn_msg *msg, struct netlink_skb_parms *nsp) {
        uint8_t response_code;
        char *response_data;

        printk(KERN_INFO "%s: %lu: idx=%x, val=%x, seq=%u, ack=%u, len=%d.\n", __func__, jiffies, msg->id.idx, msg->id.val, msg->seq, msg->ack, msg->len);
        
        response_code = msg->data[0];
        switch (response_code) {
                case RESPONSE_PAGE_OK:
                        response_data = kzalloc(CLIENT_PAGE_SIZE, GFP_ATOMIC);
                        memcpy(response_data, &msg->data[1], CLIENT_PAGE_SIZE);
                        page_recv_callback(response_data);
                        break;
                case RESPONSE_PAGE_ERR:
                        fill_with_deadbeef(&response_data, CLIENT_PAGE_SIZE);
                        page_recv_callback(response_data);
                        break;
                case RESPONSE_PAGE_SYNC_OK:
                        printk(KERN_INFO "Successfully network_msynced page.\n");
                        page_sync_recv_callback((char *)(&response_code));
                        break;
                case RESPONSE_PAGE_SYNC_ERR:
                        printk(KERN_INFO "There was an error network_msyncing a page.\n");
                        page_sync_recv_callback((char *)(&response_code));
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

static void page_sync_recv_callback(char *page_sync_resp_code) {
        g_response_data = page_sync_resp_code;
        g_response_recieved = true;
}

static void page_recv_callback(char *page_recieved) {
        g_response_data = page_recieved;
        g_response_recieved = true;
}

static bool wait_for_page_sync_response(int max_wait) {
        bool retval = true;
        int i = 0;

        while (g_response_recieved == false && i++ < max_wait) msleep(1);
        if (i > max_wait) {
                printk(KERN_INFO "Hit timeout... returning false.\n");
                retval = false;
        }
        g_response_recieved = false;

        return retval;
}

static void wait_for_page_response(int max_wait) {
        int i = 0;

        while (g_response_recieved == false && i++ < max_wait) msleep(1);
        if (i > max_wait) {
                printk(KERN_INFO "Hit timeout... filling page with DEADBEEF and returning.\n");
                // Yes this could cause problems if we ended up receiving the response
                // at some point down the line - Would need to figure out how to ignore
                // subsuquent responses to this timeout if we're writing good code...
                fill_with_deadbeef(&g_response_data, CLIENT_PAGE_SIZE);
        }
        g_response_recieved = false;
}


static int network_msync_handler(unsigned long start, size_t len, int flags)
{
    struct mm_struct *mm = current->mm;    
    struct vm_area_struct *vma;
    unsigned long end;
    unsigned long current_pos;
    unsigned long offset;
    int error = -EINVAL;
    char *nmmap_send_msg;

    // TODO: These 3 lines and the while loop may be wrong
    // I'm assuming start is physical address of the beginning
    // of the page in memory and that it is contiguous for
    // length "len" and that the pages are are constant 4Ks
    len = (len + ~PAGE_MASK) & PAGE_MASK;
    end = start + len;
    current_pos = start;

    while (current_pos < end) {
        vma = find_vma(mm, current_pos);
        if (!(vma->vm_flags & VM_SOFTDIRTY)) continue; // Don't sync the page if it isn't dirty

        offset = current_pos-start;

        printk(KERN_INFO "current_pos: %lu, offset: %lu, byte: %02x\n", current_pos, offset, *((char *)current_pos));

        // Prepare the network request
        nmmap_send_msg = kzalloc(SYNC_REQUEST_SIZE, GFP_ATOMIC);
        nmmap_send_msg[0] = REQUEST_PAGE_SYNC;
        memcpy(&nmmap_send_msg[1], &offset, PAGE_OFFSET_SIZE);
        memcpy(&nmmap_send_msg[1+PAGE_OFFSET_SIZE], (void *)current_pos, CLIENT_PAGE_SIZE);

        // Send the request away
        cn_nmmap_send_msg(nmmap_send_msg, SYNC_REQUEST_SIZE);
        if (wait_for_page_sync_response(100)) { // Wait for the response for a moment
                //vma->vm_flags &= ~VM_SOFTDIRTY; // Remove page dirty flag. TODO: This may not be correct
        }

        current_pos += CLIENT_PAGE_SIZE;
    }

    return 0;
}

static int network_mmap_fault_module_handler(struct vm_area_struct *vma, struct vm_fault *vmf) {
        char *virt_page;
        struct page *page;
        char *nmmap_send_msg;
        uint64_t faulted_page;

        faulted_page = vmf->pgoff << PAGE_SHIFT;

        printk(KERN_INFO "network_mmap_fault_module_handler: Called pgoff: %d\n", faulted_page);

        // Prepare the network request
        nmmap_send_msg = kzalloc(PAGE_REQUEST_SIZE, GFP_ATOMIC);
        nmmap_send_msg[0] = REQUEST_PAGE;
        *((uint64_t *)&nmmap_send_msg[1]) = faulted_page;

        // Send the request away
        cn_nmmap_send_msg(nmmap_send_msg, PAGE_REQUEST_SIZE);
        wait_for_page_response(100); // Wait for the response for a moment

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
        
        set_kmod_network_msync(network_msync_handler);
        set_kmod_network_mmap_fault_handler(network_mmap_fault_module_handler);
        return 0;
}


static void __exit nmmapmod_exit(void) {
        set_kmod_network_mmap_fault_handler(NULL);
        set_kmod_network_msync(NULL);
        cn_del_callback(&cn_nmmap_id);
        printk(KERN_INFO "Unloaded Network MMAP Kernel Module\n");
}

module_init(nmmapmod_init);
module_exit(nmmapmod_exit);
