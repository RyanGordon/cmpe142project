/**
 * Source code for testing our custom network_mmap syscall
 * and kernel module page fault handler
 *
 * Contributors:
 *  Ryan Gordon <rygordon4@gmail.com>
 *  Charles MacDonald <chamacd@gmail.com>
 *
 * Compile with: gcc -g -o testclient testclient.c
 * Generate object (.o) file: gcc -g -c testclient.c
 * View assembly from object file: objdump -d testclient.o
 **/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/mman.h>   

#define PAGE_SIZE 4096

// On our custom version of the 3.13.11 kernel,
// We added the syscall to the end which is entry 318
#define __NR_network_mmap 318
#define SYS_network_mmap __NR_network_mmap

 #define __NR_network_msync 319
 #define SYS_network_msync __NR_network_msync

// Need to do this here because glibc/gcc is fucked 
// and the default syscall header is not compatible
// with returning 64bit values. It turns out that
// the default syscall header is prototyped with a
// return type of long, which gcc properly decides
// to assign the output of the syscall to the %rax
// register, but improperly calls the cltq assembly
// instruction (to sign extend the 32-bit %eax 
// register) right before, messing up the return
// value. This is probably a gcc or glibc bug. Lost
// many hours of sleep over this stupid thing...
extern uint64_t syscall(long number, ...);

void print_page(char *ptr, int offset) {
    int i;
    printf("mem[%d]: ", offset);
    for (i = offset; i < (PAGE_SIZE+offset); i++) {
        printf("%02X", (uint8_t)ptr[i]);
    }
    printf("\n");
}

char *network_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags, unsigned long fd, unsigned long pgoff) {
    return (char *)syscall(SYS_network_mmap, addr, len, prot, flags, fd, pgoff);
}

int network_msync(unsigned long start, size_t len, int flags) {
    return (int)syscall(SYS_network_msync, start, len, flags);
}

int main() {
    char *p = network_mmap(NULL, (size_t)0x2000, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, (off_t)0);
    printf("Return value p: %016llX\n", (uint64_t)p);
    char *p = q;

    for (i = 0; q[i] != 0; i++) {
        q[i] = tolower(q[i]);
    }

    // The first access to each page in the follow two
    // statements will cause a page fault which should
    // be handled in our kernel module if it is loaded.
    print_page(p, 0x0000);
    print_page(p, 0x1000);

    network_msync((unsigned long)&p[0x0000], 2*PAGE_SIZE, MS_SYNC);
    printf("Done.\n");
    return 0;
}
