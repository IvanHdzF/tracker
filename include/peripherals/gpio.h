#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  GPIO_ID0_NODE,
  GPIO_ID1_NODE,
  GPIO_BOOT_NODE
} gpio_node_t;

typedef enum
{
  GPIO_LOW_STATE,
  GPIO_HIGH_STATE
} gpio_state_t;

int32_t gpio_init(void);
void gpio_set(gpio_node_t node, gpio_state_t state);

#ifdef __cplusplus
}
#endif
