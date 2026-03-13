#include "ble_stack/gap/scan.h"

#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
#if IS_ENABLED(CONFIG_BT_SCAN)
#include <bluetooth/scan.h>
typedef enum
{
  SCAN_NOT_STARTED,
  SCAN_STARTED,
} scan_state_t;

#define INTERVAL_MIN 0x6 /* 6 units, 7.5 ms, only used to setup connection */
#define INTERVAL_MAX 0xF /* 15 units, 18.75 ms, only used to setup connection */

LOG_MODULE_REGISTER(gap_scan, LOG_LEVEL_INF);

static struct bt_conn *ipg_conn;
static struct k_work scan_start_work;

static scan_state_t scan_state = SCAN_NOT_STARTED;

static void work_scan_start(struct k_work *work)
{
  int err = 0;
  if (scan_state != SCAN_STARTED)
  {
    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err)
    {
      LOG_ERR("Scanning failed to start (err %d)", err);
    }
  }
}

static void scan_filter_match(struct bt_scan_device_info *device_info, struct bt_scan_filter_match *filter_match,
                              bool connectable)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

  LOG_INF("Filters matched. Address: %s connectable: %d", addr, connectable);
}

void scan_filter_no_match(struct bt_scan_device_info *device_info, bool connectable)
{
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
  LOG_DBG("Filters NOT matched. Address: %s connectable: %d", addr, connectable);
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
  LOG_WRN("Connecting failed");
  bt_conn_unref(ipg_conn);
}

static void scan_connecting(struct bt_scan_device_info *device_info, struct bt_conn *conn)
{
  LOG_INF("Connecting...");
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match, scan_connecting_error, scan_connecting);
static struct bt_le_conn_param *conn_param = BT_LE_CONN_PARAM(INTERVAL_MIN, INTERVAL_MAX, 4, 400);

int scan_work_start(void)
{
  k_work_submit(&scan_start_work);
  return 0;
}

int scan_init(void)
{
  int err;
  struct bt_scan_init_param scan_init = {.connect_if_match = 1, .conn_param = conn_param};

  bt_scan_init(&scan_init);
  bt_scan_cb_register(&scan_cb);

  const char *name = CONFIG_BT_DEVICE_TO_CONNECT;
  // TODO: Filtereing for device name to stablish a connection is not safe for production
  // environment Remove on production in lieu of a more secure method of filtering (MAC ADDRESS,
  // UUID, etc.)
  err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, name);
  if (err)
  {
    LOG_ERR("Scanning filters cannot be set (err %d)", err);
    return err;
  }

  err = bt_scan_filter_enable(BT_SCAN_NAME_FILTER, true);
  if (err)
  {
    LOG_ERR("Filters cannot be turned on (err %d)", err);
    return err;
  }
  LOG_INF("Scan Filter added successfully");

  k_work_init(&scan_start_work, work_scan_start);
  scan_work_start();

  return err;
}

#endif /* IS_ENABLED(CONFIG_BT_SCAN) */