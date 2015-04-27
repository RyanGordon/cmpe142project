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

// On our custom version of the 3.13.11 kernel,
// We added the syscall to the end which is entry 318
#define __NR_network_mmap 318
#define SYS_network_mmap __NR_network_mmap

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

int main() {
 char *p = (char *)syscall(SYS_network_mmap, NULL, (size_t)0x2000, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, (off_t)0);
 printf("Return value p: %016llX\n", (uint64_t)p);

 // The first access to each page in the follow two
 // statements will cause a page fault which should
 // be handled in our kernel module if it is loaded.
 printf("Read mmaped page 1: %s\n", &p2[0x0000]);
 printf("Read mmaped page 2: %s\n", &p2[0x1000]);
 printf("Done.\n");
 return 0;
}
