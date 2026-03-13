#include "ble_stack/util/power.h"

#include <stdint.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "util/context.h"

LOG_MODULE_REGISTER(ble_wrapper_power, LOG_LEVEL_INF);

#define DEVICE_BEACON_TXPOWER_NUM 7

/* Threads */
#define POWER_MODULATE_THREAD_STACK_SIZE 		 512
#define POWER_MODULATE_THREAD_PRIORITY 			 12

#define BLE_SETUP_TIME_MS						 10000

static void power_read_conn_rssi(uint16_t handle, int8_t *rssi)
{
  struct net_buf *buf, *rsp = NULL;
  struct bt_hci_cp_read_rssi *cp;
  struct bt_hci_rp_read_rssi *rp;

  int32_t err;

  buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
  if (!buf)
  {
    LOG_ERR("Unable to allocate command buffer");
    return;
  }

  cp = net_buf_add(buf, sizeof(*cp));
  cp->handle = sys_cpu_to_le16(handle);

  err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
  if (err)
  {
    uint8_t reason = rsp ? ((struct bt_hci_rp_read_rssi *)rsp->data)->status : 0;
    LOG_ERR("Read RSSI err: %d reason 0x%02x", err, reason);
    return;
  }

  rp = (void *)rsp->data;
  *rssi = rp->rssi;

  net_buf_unref(rsp);
}

static void power_set_tx(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
{
  struct bt_hci_cp_vs_write_tx_power_level *cp;
  struct bt_hci_rp_vs_write_tx_power_level *rp;
  struct net_buf *buf, *rsp = NULL;
  int32_t err;

  buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(*cp));
  if (!buf)
  {
    LOG_WRN("Unable to allocate command buffer");
    return;
  }

  cp = net_buf_add(buf, sizeof(*cp));
  cp->handle = sys_cpu_to_le16(handle);
  cp->handle_type = handle_type;
  cp->tx_power_level = tx_pwr_lvl;

  err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
  if (err)
  {
    uint8_t reason = rsp ? ((struct bt_hci_rp_vs_write_tx_power_level *)rsp->data)->status : 0;
    LOG_ERR("Set Tx power - err: %d reason 0x%02x", err, reason);
    return;
  }

  rp = (void *)rsp->data;
  LOG_DBG("Tx power - type: %d handle: %d power: %d", handle_type, handle, rp->selected_tx_power);

  net_buf_unref(rsp);
}

void power_get_tx(uint8_t handle_type, uint16_t handle, int8_t *tx_pwr_lvl)
{
  struct bt_hci_cp_vs_read_tx_power_level *cp;
  struct bt_hci_rp_vs_read_tx_power_level *rp;
  struct net_buf *buf, *rsp = NULL;
  int32_t err;

  *tx_pwr_lvl = 0xFF;
  buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, sizeof(*cp));
  if (!buf)
  {
    LOG_ERR("Unable to allocate command buffer");
    return;
  }

  cp = net_buf_add(buf, sizeof(*cp));
  cp->handle = sys_cpu_to_le16(handle);
  cp->handle_type = handle_type;

  err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, buf, &rsp);
  if (err)
  {
    uint8_t reason = rsp ? ((struct bt_hci_rp_vs_read_tx_power_level *)rsp->data)->status : 0;
    LOG_ERR("Read Tx power - err: %d reason 0x%02x", err, reason);
    return;
  }

  rp = (void *)rsp->data;
  *tx_pwr_lvl = rp->tx_power_level;

  net_buf_unref(rsp);
}

static void power_modulate_adv_tx(power_adv_mod_type_t type)
{
  static uint8_t idx = 0;
  const int8_t tx_power_discovery[DEVICE_BEACON_TXPOWER_NUM] = {4, 0, -4, -8, -12, -16, -20};
  const int8_t tx_power_default = 0;

  int8_t tx_power = 0;

  /* Select tx power depending on the modulation type selected. */
  if (type == POWER_ADV_MOD_DEFAULT)
  {
    /* TODO: Check what algorithm should be used to select default power. */
    tx_power = tx_power_default;
  }
  else
  {
    tx_power = tx_power_discovery[idx];
    idx = (idx + 1) % DEVICE_BEACON_TXPOWER_NUM;
  }

  power_set_tx(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, tx_power);
}

static void power_modulate_conn_tx(conn_ctx_t *conn_ctx)
{
  int8_t rssi = 0xFF;
  int8_t txp_adaptive = 0;

  power_read_conn_rssi(conn_ctx->conn_handle, &rssi);
  if (rssi > -50)
  {
    txp_adaptive = -20;
  }
  else if (rssi > -70)
  {
    txp_adaptive = -10;
  }
  else if (rssi > -90)
  {
    txp_adaptive = -6;
  }
  else
  {
    txp_adaptive = 0;
  }

  power_set_tx(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_ctx->conn_handle, txp_adaptive);
}

void power_modulate_tx(void *p1, void *p2, void *p3)
{
	//TODO: Let a semaphore handle this 
  	/* Wait for initialization of BLE controller */
  	k_msleep(BLE_SETUP_TIME_MS);
	while (1)
	{
		/* Iterate through active connections. */
		uint8_t conn_num = 0;
		for (uint8_t i = 0; i < MAX_CONN; i++)
		{
			conn_ctx_t *conn_ctx = get_conn_ctx_from_id(i);

      if (conn_ctx == NULL)
      {
        break;
      }

      /* Only modulate Tx power for active connections. */
      if (conn_ctx->conn_status == CONN_STATUS_CONNECTED)
      {
        power_modulate_conn_tx(conn_ctx);
        conn_num++;
      }
    }

    if (conn_num == MAX_CONN)
    {
      /* Set default advertising Tx power */
      power_modulate_adv_tx(POWER_ADV_MOD_DEFAULT);
    }
    else
    {
      /* Set advertising Tx power for discovery */
      power_modulate_adv_tx(POWER_ADV_MOD_DISCOVERY);
    }

    k_sleep(K_MSEC(5000));
  }
}

/* Define used threads */
K_THREAD_DEFINE(power_modulate_tx_thread_id, POWER_MODULATE_THREAD_STACK_SIZE, power_modulate_tx, NULL, NULL, NULL,
                POWER_MODULATE_THREAD_PRIORITY, 0, 0);
