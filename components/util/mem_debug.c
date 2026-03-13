#include "util/mem_debug.h"

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mem_debug, LOG_LEVEL_DBG);

#if (DEBUG == 1)
int32_t kfree_counter = 0;
#endif

void k_free_safe(void* ptr)
{
  if (ptr != NULL)
  {
    k_free(ptr);
#if (DEBUG == 1)
    kfree_counter--;  // Decrement the counter to track memory deallocation
#endif
  }

#if (DEBUG == 1)
  if (kfree_counter < 0)
  {
    LOG_WRN("k_free called more times than k_malloc_safe, counter: %d", kfree_counter);
  }
#endif
}

void* k_malloc_safe(uint32_t size)
{
  void* ptr = k_malloc(size);
  if (ptr == NULL)
  {
    return NULL;  // Return NULL if memory allocation fails
  }
#if (DEBUG == 1)
  kfree_counter++;  // Increment the counter to track memory allocation
#endif
  return ptr;
}