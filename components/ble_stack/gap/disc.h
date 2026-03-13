#ifndef DISC_H
#define DISC_H

#include <zephyr/bluetooth/conn.h>
#include "ble_stack_types.h"
#include "variables.h"

#if IS_ENABLED(CONFIG_BT_GATT_DM)

/* Real implementations (provided in .c file) */

discovered_chrc_t *bt_gen_get_discovered_chars(void);
struct bt_conn *bt_gen_get_conn(void);

void uuid_to_uuid128(const struct bt_uuid *src, struct bt_uuid_128 *dst);

void gatt_discover(struct bt_conn *conn);

discovered_chrc_t *bt_gen_find_discovered_char_by_uuid(const struct bt_uuid_128 *uuid_128);
discovered_chrc_t *bt_gen_find_discovered_char_by_handle(uint16_t handle);

#else

/* Dummy fallbacks */

static inline discovered_chrc_t *bt_gen_get_discovered_chars(void)
{
    return NULL;
}

static inline struct bt_conn *bt_gen_get_conn(void)
{
    return NULL;
}

static inline void uuid_to_uuid128(const struct bt_uuid *src, struct bt_uuid_128 *dst)
{
    ARG_UNUSED(src);
    ARG_UNUSED(dst);
}

static inline void gatt_discover(struct bt_conn *conn)
{
    ARG_UNUSED(conn);
}

static inline discovered_chrc_t *bt_gen_find_discovered_char_by_uuid(const struct bt_uuid_128 *uuid_128)
{
    ARG_UNUSED(uuid_128);
    return NULL;
}

static inline discovered_chrc_t *bt_gen_find_discovered_char_by_handle(uint16_t handle)
{
    ARG_UNUSED(handle);
    return NULL;
}

#endif

#endif /* DISC_H */