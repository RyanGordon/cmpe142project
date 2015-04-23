/**
 * Kernel Module that registers a function to 
 * with the kernel to catch all network_mmap 
 * page faults. This allows us to do the 
 * network handling inside this kernel module
 * and iterate quickly rather then inside the
 * kernel itself and have to recompile a new
 * kernel each time, which is slow and hard
 * to debug
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
        printk(KERN_INFO "network_mmap_fault_module_handler: Called!\n");
        // TODO: Use netlink sockets here to communicate
	// 	 with a user process that's acting as an
	// 	 arbitor for over-the-network communication
	return VM_SIGFAULT_BUS; // We're not loading a page here yet so we need to tell the kernel that otherwise it will crash
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
