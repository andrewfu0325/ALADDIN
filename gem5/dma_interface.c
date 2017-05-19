#include <string.h>
#include "dma_interface.h"

#ifdef DMA_INTERFACE_V4
void* dmaLoad(void* dst_addr, void* src_addr, size_t dst_off, size_t src_off, 
              size_t size, void *enable, void *avail) {
  memmove(dst_addr + dst_off, src_addr + src_off, size);
  return NULL;
}

void* dmaStore(void* dst_addr, void* src_addr, size_t dst_off, size_t src_off, 
               size_t size, void *enable, void *avail) {
  memmove(dst_addr + dst_off, src_addr + src_off, size);
  return NULL;
}

#elif DMA_INTERFACE_V3
void* dmaLoad(void* base_addr, size_t src_off, size_t dst_off, size_t size, void *enable, void *avail) {
  memmove(base_addr + dst_off, base_addr + src_off, size);
  return NULL;
}

void* dmaStore(void* base_addr, size_t src_off, size_t dst_off, size_t size, void *enable, void *avail) {
  memmove(base_addr + dst_off, base_addr + src_off, size);
  return NULL;
}
#elif DMA_INTERFACE_V2
void* dmaLoad(void* base_addr, size_t src_off, size_t dst_off, size_t size) {
  memmove(base_addr + dst_off, base_addr + src_off, size);
  return NULL;
}

void* dmaStore(void* base_addr, size_t src_off, size_t dst_off, size_t size) {
  memmove(base_addr + dst_off, base_addr + src_off, size);
  return NULL;
}
#else
void* dmaLoad(void* addr, size_t offset, size_t size) {
  asm("");
  return NULL;
}

void* dmaStore(void* addr, size_t offset, size_t size) {
  asm("");
  return NULL;
}
#endif
void dmaFence() {
  asm("");
}
