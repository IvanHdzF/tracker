#include "cts.h"
#include "ble_stack/gatt/gatt_server.h"
#include <ble_mgr/ble_stack/util/uuids.h>
#include <ble_mgr/ble_stack_types.h>

#include <zephyr/bluetooth/uuid.h>
#include <ble_mgr/ble_stack/user_api.h>

/* =========================
 * Local storage (owned here)
 * ========================= */

static current_time_t current_time_value;


/* =========================
 * Local characteristic entries
 * ========================= */

static bt_char_entry_t cts_current_time_entry = {
    .data = &current_time_value,
    .length = sizeof(current_time_value),
};



/* =========================
 * GATT Services
 * ========================= */

BT_GATT_SERVICE_DEFINE(cts_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_CTS),

    BT_GATT_CHARACTERISTIC(BT_UUID_CTS_CURRENT_TIME,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE,
        GATT_DB_PERM_READ | GATT_DB_PERM_WRITE,
        ble_mgr_chr_read_handler,
        ble_mgr_chr_write_handler,
        &cts_current_time_entry
    ),

    BT_GATT_CCC(NULL, GATT_DB_PERM_READ | GATT_DB_PERM_WRITE),
);

