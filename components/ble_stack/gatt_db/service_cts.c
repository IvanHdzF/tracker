#include "ble_stack/gatt_db/service_cts.h"

#include "ble_stack/gatt_db/gatt_db.h"
#include "ble_stack/util/uuids.h"
#include "ble_stack_types.h"

extern bt_char_entry_t gatt_char_db[GATT_DB_MAX_ENTRIES];
/* Current Time Service Declaration */
BT_GATT_SERVICE_DEFINE(cts_service, BT_GATT_PRIMARY_SERVICE(INTERNAL_UUID_CTS),
                       BT_GATT_CHARACTERISTIC(INTERNAL_UUID_CTS_CHAR_CURRENT_TIME,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_WRITE,
                                              GATT_DB_PERM_READ | GATT_DB_PERM_WRITE, bt_gen_chr_read, bt_gen_chr_write,
                                              &gatt_char_db[GATT_DB_SVC_CTS_CHAR_CURR_TIME].data),
                       BT_GATT_CCC(NULL, GATT_DB_PERM_READ | GATT_DB_PERM_WRITE),
);

// TODO: Move out the GPS service below to a different module (app layer)
/* Location and Navigation Service */
BT_GATT_SERVICE_DEFINE(ln_service, BT_GATT_PRIMARY_SERVICE(BT_UUID_LNS),
                        BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LNF,
                                              BT_GATT_CHRC_READ,
                                              GATT_DB_PERM_READ, bt_gen_chr_read, NULL,
                                              &gatt_char_db[GATT_DB_SVC_LN_CHAR_FEAT].data),
                        BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LOC_SPD,
                                              BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_NONE, NULL, NULL,
                                              &gatt_char_db[GATT_DB_SVC_LN_CHAR_GPS].data),
                        BT_GATT_CCC(NULL, GATT_DB_PERM_READ | GATT_DB_PERM_WRITE),
                        BT_GATT_CHARACTERISTIC(BT_UUID_GATT_PQ,
                                              BT_GATT_CHRC_READ,
                                              GATT_DB_PERM_READ, bt_gen_chr_read, NULL,
                                              &gatt_char_db[GATT_DB_SVC_LN_CHAR_PQ].data),
);
