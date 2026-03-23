#include "ln.h"

#include "ble_stack/gatt/gatt_server.h"
#include <ble_mgr/ble_stack/util/uuids.h>
#include <ble_mgr/ble_stack_types.h>

#include <zephyr/bluetooth/uuid.h>
#include <ble_mgr/ble_stack/user_api.h>

/* =========================
 * Local storage (owned here)
 * ========================= */

static uint16_t ln_feat;
static loc_and_speed_data_t location_and_speed_data;
static pos_qual_t pos_qual_data;

/* =========================
 * Local characteristic entries
 * ========================= */

static bt_char_entry_t ln_feat_entry = {
    .data = &ln_feat,
    .length = sizeof(ln_feat),
};

static bt_char_entry_t ln_gps_entry = {
    .data = &location_and_speed_data,
    .length = sizeof(location_and_speed_data),
};

static bt_char_entry_t ln_pq_entry = {
    .data = &pos_qual_data,
    .length = sizeof(pos_qual_data),
};

/* =========================
 * GATT Services
 * ========================= */

BT_GATT_SERVICE_DEFINE(ln_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_LNS),

    BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LNF,
        BT_GATT_CHRC_READ,
        GATT_DB_PERM_READ,
        ble_mgr_chr_read_handler,
        NULL,
        &ln_feat_entry
    ),

    BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LOC_SPD,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL,
        NULL,
        &ln_gps_entry
    ),

    BT_GATT_CCC(NULL, GATT_DB_PERM_READ | GATT_DB_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BT_UUID_GATT_PQ,
        BT_GATT_CHRC_READ,
        GATT_DB_PERM_READ,
        ble_mgr_chr_read_handler,
        NULL,
        &ln_pq_entry
    ),
);
