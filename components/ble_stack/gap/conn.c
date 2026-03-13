#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

#include "ble_stack/gap/adv.h"
#include "ble_stack/gap/disc.h"
#include "ble_stack/gap/scan.h"
#include "ble_stack/gatt_db/gatt_db.h"
#include "ble_stack/l2capcoc/coc_transport.h"
#include "peripherals/gpio.h"
#include "util/context.h"
#include "ble_stack/gap/conn.h"

LOG_MODULE_REGISTER(gap_conn, LOG_LEVEL_DBG);

#define DATA_LEN_UPDATE_DEFAULT 251
#define DATA_LEN_UPDATE_TIME 2120



/* Passkey */
#define BLE_FIXED_PASSKEY 123456

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err);
static void disconnected(struct bt_conn *conn, uint8_t reason);
static void param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout);
#if IS_ENABLED(CONFIG_BT_USER_DATA_LEN_UPDATE)
static void phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *param);
#endif // CONFIG_BT_USER_DATA_LEN_UPDATE
#if IS_ENABLED(CONFIG_BT_USER_DATA_LEN_UPDATE)
static void data_len_updated(struct bt_conn *conn, struct bt_conn_le_data_len_info *info);
#endif // CONFIG_BT_USER_DATA_LEN_UPDATE

#if IS_ENABLED(CONFIG_BT_GATT_CLIENT)
static void mtu_updated(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params);
#endif // CONFIG_BT_GATT_CLIENT
#if IS_ENABLED(CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION)
static void on_security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err);
#endif

#define PER_DEV_INTERVAL_MIN 0x6 /* 6 units, 7.5 ms, only used to setup connection */
#define PER_DEV_INTERVAL_MAX 0x6 /* 6 units, 7.5 ms, only used to setup connection */
#define PER_DEV_LATENCY 0x0 /* 0 units, Very important */
#define PER_DEV_TIMEOUT 400

#define CEN_DEV_INTERVAL_MIN 0x07 /* 7 units, 8.75 ms */
#define CEN_DEV_INTERVAL_MAX 0x0A /* 10 units, 12.50  ms */
#define CEN_DEV_LATENCY 0x02      /* Up to 2 skipped events in a row to avoid collisions */
#define CEN_DEV_TIMEOUT 400

#define MAX_CONN_PARAM_UPDATE_ATTEMPTS 3

static uint8_t conn_param_update_attempts = 0;
/* Connection params for peripheral devices, shorter intervals and more throughput */
static struct bt_le_conn_param *peripheral_dev_conn_param = BT_LE_CONN_PARAM(PER_DEV_INTERVAL_MIN, PER_DEV_INTERVAL_MAX, PER_DEV_LATENCY, PER_DEV_TIMEOUT);

/* Connection params for central devices, the priority is lower than peripheral conns */
static struct bt_le_conn_param *central_dev_conn_param = BT_LE_CONN_PARAM(CEN_DEV_INTERVAL_MIN, CEN_DEV_INTERVAL_MAX, CEN_DEV_LATENCY, CEN_DEV_TIMEOUT);

/* BLE connection callback definition */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .le_param_updated = param_updated,
    #if IS_ENABLED(CONFIG_BT_USER_DATA_LEN_UPDATE)
    .le_phy_updated = phy_updated,
    .le_data_len_updated = data_len_updated,
    #endif // CONFIG_BT_USER_DATA_LEN_UPDATE
    #if IS_ENABLED(CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION)
    .security_changed = on_security_changed,
    #endif
};

/* Exchange params callbacks */
#if defined(CONFIG_BT_GATT_CLIENT)
static struct bt_gatt_exchange_params ep = {
    .func = mtu_updated,
};
/* MTU Update callback */
static void mtu_updated(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params)
{
  if (!err)
  {
    uint16_t mtu = bt_gatt_get_mtu(conn) - 3;
    LOG_INF("MTU updated: %d", mtu);
  }
  else
  {
    LOG_ERR("MTU update failed");
  }
}
#endif



#if IS_ENABLED(CONFIG_BT_USER_DATA_LEN_UPDATE)
static void data_len_updated(struct bt_conn *conn, struct bt_conn_le_data_len_info *info)
{
  LOG_INF("Data length updated Rx: %u Tx: %u", info->rx_max_len, info->tx_max_len);
}
#endif // CONFIG_BT_USER_DATA_LEN_UPDATE

#if IS_ENABLED(CONFIG_BT_USER_PHY_UPDATE)
static void phy_updated(struct bt_conn *conn, struct bt_conn_le_phy_info *param)
{
  LOG_INF("Phy updated Rx: %s Tx: %s",
          (param->rx_phy == BT_GAP_LE_PHY_2M)   ? "2M"
          : (param->rx_phy == BT_GAP_LE_PHY_1M) ? "1M"
                                                : "Coded",
          (param->tx_phy == BT_GAP_LE_PHY_2M)   ? "2M"
          : (param->tx_phy == BT_GAP_LE_PHY_1M) ? "1M"
                                                : "Coded");
}
#endif // CONFIG_BT_USER_PHY_UPDATE

static void param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout)
{
  uint16_t interval_ms = interval * 1.25;
  uint16_t latency_ms = latency * interval_ms;
  uint16_t timeout_ms = timeout * 10;
  LOG_INF("Conn params updated: interval %d, latency %d, timeout %d", interval_ms, latency_ms, timeout_ms);

  struct bt_conn_info info = {0};
  if (bt_conn_get_info(conn, &info) != 0)
  {
    LOG_ERR("Error obtaining information from conn");
  }

  uint8_t role = info.role;
  if (role != BT_CONN_ROLE_CENTRAL)
  {
    LOG_DBG("Skipping check because role is NOT central");
    return;
  }

  if (interval > PER_DEV_INTERVAL_MAX && conn_param_update_attempts < MAX_CONN_PARAM_UPDATE_ATTEMPTS)
  {
    LOG_WRN("Central role detected, interval %d exceeds max %d, updating connection parameters", interval, PER_DEV_INTERVAL_MAX);
    bt_conn_le_param_update(conn, peripheral_dev_conn_param);
    conn_param_update_attempts++;
  }
}

static void connected(struct bt_conn *conn, uint8_t status)
{
  if (status)
  {
    LOG_ERR("Connection failed (err 0x%02x)", status);
    return;
  }
  else
  {
    LOG_INF("Connected!");
  }

  char addr[BT_ADDR_LE_STR_LEN];

  /* Get role to determine type of device which was connected */
  struct bt_conn_info info = {0};
  if (bt_conn_get_info(conn, &info) != 0)
  {
    LOG_ERR("Error obtaining information from conn");
  }

  uint8_t role = info.role;
  LOG_INF("Connection Interval: %d", info.le.interval_us);
  LOG_INF("Peripheral lat: %d", info.le.latency);

  #if IS_ENABLED(CONFIG_BT_USER_DATA_LEN_UPDATE)
  LOG_INF("TX data_len: %d", info.le.data_len->tx_max_len);
  LOG_INF("RX data_len: %d", info.le.data_len->rx_max_len);
  #endif
  uint16_t mtu = bt_gatt_get_mtu(conn) - 3;  // 3 bytes used for Attribute headers.
  LOG_INF("MTU: %d", mtu);
  conn_ctx_t *conn_ctx = get_conn_ctx_available();

  if (conn_ctx == NULL)
  {
    LOG_ERR("No available connection context for new connection");
    // TODO: Consider disconnecting if no context is available, otherwise the system can be put in a bad state with a connection that cannot be handled
    return;
  }

  /* Set connection context parameters. */
  conn_ctx->conn = bt_conn_ref(conn);
  bt_hci_get_conn_handle(conn_ctx->conn, &(conn_ctx->conn_handle));

  bt_addr_le_to_str(bt_conn_get_dst(conn_ctx->conn), addr, sizeof(addr));
  LOG_INF("Connected %s - handle %d", addr, conn_ctx->conn_handle);

  /* Assign connection data */
  conn_ctx->conn_status = CONN_STATUS_CONNECTED;
  conn_ctx->data.gpio_pin = conn_ctx->id;

  if (role == BT_CONN_ROLE_CENTRAL)
  {
    LOG_INF("Connected as CENTRAL");
    conn_param_update_attempts = 0;
    bt_conn_le_param_update(conn_ctx->conn, peripheral_dev_conn_param);
    #if IS_ENABLED(CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION)
    bt_conn_set_security(conn_ctx->conn, BT_SECURITY_L4);
    #endif
    
  }
  else if (role == BT_CONN_ROLE_PERIPHERAL)
  {
    conn_param_update_attempts = 0;
    bt_conn_le_param_update(conn_ctx->conn, central_dev_conn_param);
    LOG_INF("Connected as PERIPHERAL");
  }
  else
  {
    LOG_WRN("Connected with unknown role (%d)", role);
  }
  // TODO: Add CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION check here to attempt discovery, otherwise it can be used as an attack vector
  // This shall be done after feasibility study
  gatt_discover(conn);

  

  #if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
  /* Update data length parameters */
  struct bt_conn_le_data_len_param data_len_param = {
      .tx_max_len = DATA_LEN_UPDATE_DEFAULT,
      .tx_max_time = DATA_LEN_UPDATE_TIME,
  };
  int err = bt_conn_le_data_len_update(conn, &data_len_param);
  if (err)
  {
    LOG_ERR("Data length update failed (err 0x%02x)", err);
  }
  #endif // CONFIG_BT_USER_DATA_LEN_UPDATE

  /* Update MTU */
  #if defined(CONFIG_BT_GATT_CLIENT)
  err = bt_gatt_exchange_mtu(conn, &ep);

  if (err)
  {
    LOG_ERR("MTU exchange request failed (err 0x%02x)", err);
  }
  #endif // CONFIG_BT_GATT_CLIENT

  
  #if defined(CONFIG_BT_USER_PHY_UPDATE)
  /* Update PHY to preferred phy */
  struct bt_conn_le_phy_param phy_params = {
      .options = BT_CONN_LE_PHY_OPT_NONE,
      .pref_tx_phy = BT_GAP_LE_PHY_2M,
      .pref_rx_phy = BT_GAP_LE_PHY_2M,
  };

  err = bt_conn_le_phy_update(conn, &phy_params);
  if (err)
  {
    LOG_ERR("PHY update request failed (err 0x%02x)", err);
  }
  #endif // CONFIG_BT_USER_PHY_UPDATE

}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
  /* Get role to determine type of device which was disconnected */
  struct bt_conn_info info = {0};
  if (bt_conn_get_info(conn, &info) != 0)
  {
    LOG_ERR("Error obtaining information from conn");
  }
  uint8_t role = info.role;

  conn_ctx_t *conn_ctx = get_conn_ctx_from_ref(conn);

  if (conn_ctx == NULL)
  {
    LOG_ERR("No connection context found for disconnected connection");
    return;
  }
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn_ctx->conn), addr, sizeof(addr));
  LOG_INF("Disconnected: %s - handle %d (reason %u)", addr, conn_ctx->conn_handle, reason);

  /* Dereference connection */
  if (conn_ctx->conn)
  {
    bt_conn_unref(conn_ctx->conn);
    conn_ctx->conn = NULL;
  }
 
  /* Deassign data */
  conn_ctx->conn_status = CONN_STATUS_DISCONNECTED;

  memset(conn_ctx, 0, sizeof(conn_ctx_t));
  /* Resume work accordingly */
  if (role == BT_CONN_ROLE_CENTRAL)
  {
    LOG_INF("Disconnected from PERIPHERAL");
    scan_work_start();
  }
  else if (role == BT_CONN_ROLE_PERIPHERAL)
  {
    LOG_INF("Disconnected from CENTRAL");
    adv_work_start();
  }
  else
  {
    LOG_WRN("Disconnected with unknown role (%d)", role);
  }
}

#if IS_ENABLED(CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION)


/* Bonding */
static void on_security_changed(struct bt_conn *conn, bt_security_t level,
 enum bt_security_err err)
{
 char addr[BT_ADDR_LE_STR_LEN];  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
 char reason[BT_ADDR_LE_STR_LEN];

 memcpy(reason, bt_security_err_to_str(err), sizeof(reason));
 if (!err) {
  LOG_INF("Security changed: %s level %u\n", addr, level);
 } else {
  LOG_INF("Security failed: %s level %u err %d\nReason: %s", addr, level,
  err, reason);
 }
}

/* Pairing helpers*/
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
 char addr[BT_ADDR_LE_STR_LEN];
 bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
 LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
 char addr[BT_ADDR_LE_STR_LEN];
 bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
 LOG_INF("Pairing cancelled: %s\n", addr);
}

void auth_passkey_entry(struct bt_conn *conn)
{
 char addr[BT_ADDR_LE_STR_LEN];
 bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
 LOG_INF("Passkey entry requested: %s\n", addr);
 bt_conn_auth_passkey_entry(conn, BLE_FIXED_PASSKEY);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
 .passkey_display = auth_passkey_display,
 .cancel = auth_cancel,
 .passkey_entry = auth_passkey_entry
};

#endif // CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION

int ble_bonding_init(void)
{
  #if IS_ENABLED(CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION)
  int err = 0;
  err = bt_passkey_set(BLE_FIXED_PASSKEY);
  if (err) {
    LOG_ERR("Failed to set passkey (err: %d)", err);
    return err;
  }

  err = bt_conn_auth_cb_register(&conn_auth_callbacks);
  if (err) {
    LOG_INF("Failed to register authorization callbacks.\n");
  }
  return err;
  #else
  return 0;
  #endif
}

