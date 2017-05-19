#ifndef __DMA_INTERFACE_H__
#define __DMA_INTERFACE_H__

#include <stddef.h>

#define PAGE_SIZE 4096

#ifdef DMA_INTERFACE_V4
// Version 4 of the DMA interface separate the source and the destination buffer,
// which are the scratchpad address space and the cpu addrses space
void* dmaLoad(void* dst_addr, void* src_addr, size_t dst_off, size_t src_off, 
              size_t size, void *enable, void *avail);
void* dmaStore(void* dst_addr, void* src_addr, size_t dst_off, size_t src_off, 
               size_t size, void *enable, void *avail);

#elif DMA_INTERFACE_V3
// Version 3 of the DMA interface add a enable signal from CPU side to 
// Notify that current dmaLoad is avaialable to mainatain the consistency

void* dmaLoad(void* base_addr, size_t src_off, size_t dst_off, size_t size, void *enable, void *avail);
void* dmaStore(void* base_addr, size_t src_off, size_t dst_off, size_t size, void *enable, void *avail);
#elif DMA_INTERFACE_V2
// Version 2 of the DMA interface separates source and destination offsets from
// the base address into different fields, and on the host machine, memory is
// actually copied from source to destination (the memory copy will not show up
// in the trace).

void* dmaLoad(void* base_addr, size_t src_off, size_t dst_off, size_t size);
void* dmaStore(void* base_addr, size_t src_off, size_t dst_off, size_t size);
#else
// Version 1 of the DMA interface is now deprecated and will be removed entirely.

void* dmaLoad(void *addr, size_t offset, size_t size);
void* dmaStore(void *addr, size_t offset, size_t size);
#endif
void dmaFence();

#endif
