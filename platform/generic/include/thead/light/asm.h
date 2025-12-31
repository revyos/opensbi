
#ifndef __RISCV_THEAD_LIGHT_ASM_H__
#define __RISCV_THEAD_LIGHT_ASM_H__

#define dcache_call()                                             \
	do {                                              \
		asm volatile(".long 0x0010000b\n"); \
	} while (0)

#define dcache_ciall()                                             \
	do {                                              \
		asm volatile(".long 0x0030000b\n"); \
	} while (0)

#define sync_is()   asm volatile (".long 0x01b0000b")

#endif