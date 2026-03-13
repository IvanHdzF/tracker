#ifndef MEM_DEBUG_H
#define MEM_DEBUG_H

#include <stdint.h>
void k_free_safe(void* ptr);
void* k_malloc_safe(uint32_t size);

#endif /* MEM_DEBUG_H */