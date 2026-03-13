#ifndef POWER_H
#define POWER_H

#include <stdint.h>

typedef enum
{
  POWER_ADV_MOD_DEFAULT,
  POWER_ADV_MOD_DISCOVERY
} power_adv_mod_type_t;

void power_get_tx(uint8_t handle_type, uint16_t handle, int8_t *tx_pwr_lvl);
void power_modulate_tx(void *p1, void *p2, void *p3);

#endif /* POWER_H */