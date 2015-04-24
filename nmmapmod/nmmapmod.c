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

#define DRIVER_AUTHOR "Ryan Gordon <rygorde4@gmail.com>, Charles MacDonald <chamacd@gmail.com>"
#define DRIVER_DESC   "Networked mmap page fault handler"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

static int network_mmap_fault_module_handler(struct vm_area_struct *vma, struct vm_fault *vmf) {
        char *virt_page;
        struct page *page;

        printk(KERN_INFO "network_mmap_fault_module_handler: Called!\n");
        // TODO: Use netlink sockets here to communicate
	// 	 with a user process that's acting as an
	// 	 arbitor for over-the-network communication

        // Example code for creating a page in memory and filling it with "DEADBEEF" and returning it to the faulted page
        virt_page = (char *)get_zeroed_page(GFP_USER);
        memcpy(virt_page, "DEADBEEF", 8);

        page = virt_to_page(virt_page);
        get_page(page) // Increments reference count of page
        vmf->page = page;
        printk(KERNINFO "virt_page: %016llX, page: %016llX, vmf->page: %016llX\n", (char)virt_page, (char)page, (char)vmf->page);

        return 0; // If a page isn't being loaded then you need to return VM_SIGFAULT_BUS otherwise the kernel itself will crash
}

static int __init nmmapmod_init(void) {
        printk(KERN_INFO "Loading Network MMAP Kernel Module with page fault handler\n");
        set_kmod_network_mmap_fault_handler(network_mmap_fault_module_handler);
        return 0;
}

static void __exit nmmapmod_exit(void) {
        set_kmod_network_mmap_fault_handler(NULL);
        printk(KERN_INFO "Unloading Network MMAP Kernel Module\n");
}

module_init(nmmapmod_init);
module_exit(nmmapmod_exit);
