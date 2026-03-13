#include "ble_stack/gatt_db/service_di.h"

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>

#include "ble_stack/util/uuids.h"
#include "ble_stack/gatt_db/gatt_db.h"

/* Device Information Service variables */
// BUILD_ASSERT(sizeof(CONFIG_BT_DEV_DIS_MODEL) <= CONFIG_BT_DEV_DIS_STR_MAX + 1);
// BUILD_ASSERT(sizeof(CONFIG_BT_DEV_DIS_MANUF) <= CONFIG_BT_DEV_DIS_STR_MAX + 1);
static uint8_t dis_model[CONFIG_BT_DEV_DIS_STR_MAX + 1] = CONFIG_BT_DEV_DIS_MODEL;
static uint8_t dis_manuf[CONFIG_BT_DEV_DIS_STR_MAX + 1] = CONFIG_BT_DEV_DIS_MANUF;
// BUILD_ASSERT(sizeof(CONFIG_BT_DEV_DIS_FW_REV_STR) <= CONFIG_BT_DEV_DIS_STR_MAX + 1);
static uint8_t dis_fw_rev[CONFIG_BT_DEV_DIS_STR_MAX + 1] = CONFIG_BT_DEV_DIS_FW_REV_STR;
static uint8_t dis_sw_rev[CONFIG_BT_DEV_DIS_STR_MAX + 1] = CONFIG_BT_DEV_DIS_SW_REV_STR;
// BUILD_ASSERT(sizeof(CONFIG_BT_DEV_DIS_SERIAL_NUMBER_STR) <= CONFIG_BT_DEV_DIS_STR_MAX + 1);
static uint8_t dis_serial_number[CONFIG_BT_DEV_DIS_STR_MAX + 1] = CONFIG_BT_DEV_DIS_SERIAL_NUMBER_STR;


static ssize_t read_str(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
  return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data, strlen(attr->user_data));
}

/* Device Information Service Declaration */
BT_GATT_SERVICE_DEFINE(di_service, BT_GATT_PRIMARY_SERVICE(INTERNAL_UUID_DIS),
                       BT_GATT_CHARACTERISTIC(INTERNAL_UUID_DIS_CHAR_MODEL, BT_GATT_CHRC_READ, GATT_DB_PERM_READ,
                                              read_str, NULL, &dis_model),
                       BT_GATT_CHARACTERISTIC(INTERNAL_UUID_DIS_CHAR_MANUFACTURER, BT_GATT_CHRC_READ, GATT_DB_PERM_READ,
                                              read_str, NULL, &dis_manuf),
                       BT_GATT_CHARACTERISTIC(INTERNAL_UUID_DIS_CHAR_FW_VER, BT_GATT_CHRC_READ, GATT_DB_PERM_READ,
                                              read_str, NULL, &dis_fw_rev),
                       BT_GATT_CHARACTERISTIC(INTERNAL_UUID_DIS_CHAR_SW_VER, BT_GATT_CHRC_READ, GATT_DB_PERM_READ,
                                              read_str, NULL, &dis_sw_rev),
                       BT_GATT_CHARACTERISTIC(INTERNAL_UUID_DIS_CHAR_SN, BT_GATT_CHRC_READ, GATT_DB_PERM_READ, read_str,
                                              NULL, &dis_serial_number),);
