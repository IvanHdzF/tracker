#include "ble_stack/user_api.h"

#include <string.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "ble_stack/gap/adv.h"
#include "ble_stack/gap/disc.h"
#include "ble_stack/gap/scan.h"
#include "ble_stack/gatt_db/gatt_client.h"
#include "ble_stack/gatt_db/gatt_db.h"
#include "ble_stack_types.h"

LOG_MODULE_REGISTER(ble_user_api, LOG_LEVEL_DBG);

/*----------- GAP user functions -------------*/

int ble_gap_adv_start(void)
{
  int err = adv_start();
  return err;
}

int ble_gap_scan_init(void)
{
  int err = scan_init();
  return err;
}

/*----------- GATT user functions -------------*/

int ble_gatt_get_char_value_and_len(const struct bt_uuid *uuid, void *value, uint16_t *len)
{
  ssize_t ret = bt_get_attr_value_and_len(uuid, value, len);
  return (ret < 0) ? (int)ret : 0;
}

int ble_gatt_set_gen_rx_notification_cb(gen_rx_notif_cb_func cb)
{
  ssize_t ret = bt_gen_chr_set_gen_rx_cb(cb);
  return (ret < 0) ? (int)ret : 0;
}

int ble_gatt_set_read_cb(const struct bt_uuid *uuid, bt_gatt_handler_t cb)
{
  ssize_t ret = bt_gen_chr_set_read_cb(uuid, cb);
  return (ret < 0) ? (int)ret : 0;
}

int ble_gatt_set_write_cb(const struct bt_uuid *uuid, bt_gatt_handler_t cb)
{
  ssize_t ret = bt_gen_chr_set_write_cb(uuid, cb);
  return (ret < 0) ? (int)ret : 0;
}

int ble_gatt_set_notify_tx_cb(const struct bt_uuid *uuid, bt_gatt_handler_t cb)
{
  ssize_t ret = bt_gen_chr_set_notify_tx_cb(uuid, cb);
  return (ret < 0) ? (int)ret : 0;
}

int ble_gatt_notify(const struct bt_uuid *uuid, const void *data, uint16_t len)
{
  ssize_t ret = bt_gen_chr_notify_tx(uuid, data, len);
  return (ret < 0) ? (int)ret : 0;
}

int ble_gatt_services_init(void)
{
  int err = 0;

  /* Nothing to do here yet */
  return err;
}

int ble_gatt_read_chrc(const struct bt_uuid *uuid)
{
  struct bt_uuid_128 uuid_128 = {0};
  uuid_to_uuid128(uuid, &uuid_128);
  return bt_client_gen_chrc_do_op(&uuid_128, OP_READ, NULL, 0);
}

int ble_gatt_write_chrc(const struct bt_uuid *uuid, const void *data, const uint16_t len)
{
  struct bt_uuid_128 uuid_128 = {0};
  uuid_to_uuid128(uuid, &uuid_128);
  return bt_client_gen_chrc_do_op(&uuid_128, OP_WRITE, data, len);
}

int ble_gatt_write_no_resp_chrc(const struct bt_uuid *uuid, const void *data, const uint16_t len)
{
  struct bt_uuid_128 uuid_128 = {0};
  uuid_to_uuid128(uuid, &uuid_128);
  return bt_client_gen_chrc_do_op(&uuid_128, OP_WRITE_WITHOUT_RESP, data, len);
}