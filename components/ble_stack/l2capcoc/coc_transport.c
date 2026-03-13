#include "coc_transport.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>

#if IS_ENABLED(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)

#include "ble_stack/user_api.h"
#include "zephyr/bluetooth/l2cap.h"
#include "zephyr/kernel.h"
#include "zephyr/logging/log.h"
#include "zephyr/logging/log_msg.h"
#include "zephyr/net_buf.h"

LOG_MODULE_REGISTER(l2capcoc, LOG_LEVEL_DBG);

#define POOL_SIZE (1024)
#define POOL_COUNT (255)
#define L2CAP_PSM_NUM 0x0081
#define COC_CREDITS_DEFAULT 20

#if IS_ENABLED(CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION)
#define GAP_SECURITY BT_SECURITY_L4

#else
#define GAP_SECURITY BT_SECURITY_L1
#endif

NET_BUF_POOL_DEFINE(send_buffer_pool, (COC_CREDITS_DEFAULT * 5), BT_L2CAP_BUF_SIZE(BT_L2CAP_SDU_TX_MTU), POOL_COUNT, NULL);


static struct bt_l2cap_server l2cap_server;

/* As server */
// static void l2cap_connected_cb(struct bt_l2cap_chan *chan)
// {
//     printk("[INFO] L2CAP channel connected\n");
// }

// static void l2cap_disconnected_cb(struct bt_l2cap_chan *chan)
// {
//     printk("[INFO] L2CAP channel disconnected\n");
// }

// static int l2cap_receive_cb(struct bt_l2cap_chan *chan, struct net_buf *buffer)
// {
//   if (buffer->len < 4) {
//       printk("[WARN] Received packet too short\n");
//       net_buf_unref(buffer);
//       return -EINVAL;
//   }

//   uint32_t tx_timestamp;
//   memcpy(&tx_timestamp, buffer->data, sizeof(tx_timestamp));

//   uint32_t rx_timestamp = k_uptime_get();

//   static uint32_t last_rx = 0;
//   static uint32_t last_tx = 0;

//   uint32_t delta_rx = last_rx ? (rx_timestamp - last_rx) : 0;
//   uint32_t delta_tx = last_tx ? (tx_timestamp - last_tx) : 0;

//   printk("[RECV] RX=%04u ms, +%02u ms | TX=%04u ms, +%02u ms | Payload=%u bytes\n",
//           rx_timestamp, delta_rx,
//           tx_timestamp, delta_tx,
//           buffer->len - sizeof(tx_timestamp));

//   last_rx = rx_timestamp;
//   last_tx = tx_timestamp;

//   net_buf_unref(buffer);
//   return 0;
// }


// static const struct bt_l2cap_chan_ops l2cap_ops = {
//     .connected = l2cap_connected_cb,
//     .disconnected = l2cap_disconnected_cb,
//     .recv = l2cap_receive_cb,
// };




/* As client */
typedef struct
{
  struct bt_l2cap_le_chan l2cap_chan;
  coc_api_rx_callback rx_callback;
} l2cap_ctx_t;

static l2cap_ctx_t l2cap_coc_api_ctx;

static uint8_t default_coc_callback(const void *data, uint16_t len)
{
  uint32_t tx_timestamp;
  memcpy(&tx_timestamp, data, sizeof(tx_timestamp));

  uint32_t rx_timestamp = k_uptime_get();

  static uint32_t last_rx = 0;
  static uint32_t last_tx = 0;

  uint32_t delta_rx = last_rx ? (rx_timestamp - last_rx) : 0;
  uint32_t delta_tx = last_tx ? (tx_timestamp - last_tx) : 0;

  LOG_INF("[RECV] RX=%04u ms, +%02u ms | TX=%04u ms, +%02u ms | Payload=%u bytes\n", rx_timestamp, delta_rx,
          tx_timestamp, delta_tx, len - sizeof(tx_timestamp));

  last_rx = rx_timestamp;
  last_tx = tx_timestamp;

  return COC_ERR_OK;
}

static inline struct bt_l2cap_le_chan *ble_coc_get_channel(void)
{
  return &l2cap_coc_api_ctx.l2cap_chan;
}

static inline coc_api_rx_callback *ble_coc_get_callback_pt(void)
{
  return &l2cap_coc_api_ctx.rx_callback;
}

static void l2cap_connected_cb(struct bt_l2cap_chan *chan)
{
  LOG_INF("L2CAP channel connected");
}

static void l2cap_disconnected_cb(struct bt_l2cap_chan *chan)
{
  LOG_INF("L2CAP channel disconnected");
}

static int l2cap_receive_cb(struct bt_l2cap_chan *chan, struct net_buf *buffer)
{
  if (buffer->len < 4)
  {
    LOG_INF("[WARN] Received packet too short\n");
    net_buf_unref(buffer);

    return -EINVAL;
  }

  coc_api_rx_callback *cb = ble_coc_get_callback_pt();

  if (*cb)
  {
    (*cb)(buffer->data, buffer->len);
  }
  return 0;
}

static const struct bt_l2cap_chan_ops l2cap_ops = {
    .connected = l2cap_connected_cb,
    .disconnected = l2cap_disconnected_cb,
    .recv = l2cap_receive_cb,
};

coc_api_error_t ble_coc_init(void)
{
  struct bt_l2cap_le_chan *l2cap_chan;
  l2cap_chan = ble_coc_get_channel();
  // Initialize the L2CAP CoC channel
  memset(l2cap_chan, 0, sizeof(struct bt_l2cap_le_chan));
  l2cap_chan->chan.ops = &l2cap_ops;
  l2cap_chan->rx.mtu = BT_L2CAP_SDU_RX_MTU;
  l2cap_chan->tx.mtu = BT_L2CAP_SDU_TX_MTU;
  l2cap_chan->rx.credits = COC_CREDITS_DEFAULT;
  l2cap_chan->tx.credits = COC_CREDITS_DEFAULT;

  coc_api_rx_callback *prev_cback = ble_coc_get_callback_pt();
  *prev_cback = default_coc_callback;

  return COC_ERR_OK;
}

coc_api_error_t ble_coc_connect(struct bt_conn *conn)
{
  if (!conn)
  {
    LOG_ERR("Invalid connection\n");
    return COC_ERR_ALLOC;
  }

  struct bt_l2cap_le_chan *l2cap_chan;
  l2cap_chan = ble_coc_get_channel();

  // Initialize the L2CAP CoC channel
  int err = bt_l2cap_chan_connect(conn, &l2cap_chan->chan, L2CAP_PSM_NUM);
  if (err < 0)
  {
    LOG_ERR("L2CAP connect failed (%d)\n", err);
    return COC_ERR_NOT_SUPPORTED;
  }

  LOG_INF("CoC channel connected\n");
  return COC_ERR_OK;
}

coc_api_error_t ble_coc_send_data(uint8_t *data, size_t len)
{
  if (len + sizeof(uint32_t) > BT_L2CAP_SDU_TX_MTU)
  {
    LOG_ERR("Data length exceeds MTU\n");
    return COC_ERR_INVALID_PARAM;
  }

  struct bt_l2cap_le_chan *l2cap_chan;
  l2cap_chan = ble_coc_get_channel();

  // Check if there is an active CoC channel
  if (l2cap_chan->state != BT_L2CAP_CONNECTED)
  {
    LOG_ERR("No active CoC channel\n");
    return COC_ERR_NOT_SUPPORTED;
  }

  // Prepare packet with timestamp
  uint8_t packet[BT_L2CAP_SDU_TX_MTU] = {0};

  uint32_t timestamp = (uint32_t)k_uptime_get();  // truncate to 4 bytes

  // Copy 4-byte timestamp into packet
  memcpy(packet, &timestamp, sizeof(timestamp));
  memcpy(packet + sizeof(timestamp), data, len);

  struct net_buf *buf = net_buf_alloc_len(&send_buffer_pool, BT_L2CAP_BUF_SIZE(len + sizeof(timestamp)), K_FOREVER);
  if (!buf)
  {
    LOG_ERR("net_buf_alloc failed\n");
    return COC_ERR_ALLOC;
  }

  net_buf_reserve(buf, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
  net_buf_add_mem(buf, packet, (len + sizeof(timestamp)));  // Add timestamp and data

  int err = bt_l2cap_chan_send(&l2cap_chan->chan, buf);
  if (err < 0)
  {
    LOG_ERR("Failed to send data (err: %d)\n", err);
    net_buf_unref(buf);
    return COC_ERR_BUSY;
  }

  LOG_DBG("Data sent successfully, length: %d", len);
  return COC_ERR_OK;
}

coc_api_error_t ble_coc_register_rx_callback(coc_api_rx_callback callback)
{
  if (callback == NULL)
  {
    return COC_ERR_INVALID_PARAM;
  }

  coc_api_rx_callback *prev_cback = ble_coc_get_callback_pt();
  *prev_cback = callback;

  return COC_ERR_OK;
}

/* As a server */

static int l2cap_accept_cb(struct bt_conn *conn, struct bt_l2cap_server * server, struct bt_l2cap_chan **chan)
{
  LOG_INF("[INFO] Incoming L2CAP connection\n");

  struct bt_l2cap_le_chan *l2cap_chan;
  l2cap_chan = ble_coc_get_channel();
  // Initialize the L2CAP CoC channel
  memset(l2cap_chan, 0, sizeof(struct bt_l2cap_le_chan));
  l2cap_chan->chan.ops = &l2cap_ops;
  l2cap_chan->rx.mtu = BT_L2CAP_SDU_RX_MTU;
  l2cap_chan->tx.mtu = BT_L2CAP_SDU_TX_MTU;
  l2cap_chan->rx.credits = COC_CREDITS_DEFAULT;
  l2cap_chan->tx.credits = COC_CREDITS_DEFAULT;

  *chan = &l2cap_chan->chan;
  return 0;
}

coc_api_error_t ble_coc_init_server(void)
{
    /* Initialize BLE CoC Server */
  l2cap_server.psm = L2CAP_PSM_NUM;
  l2cap_server.sec_level = GAP_SECURITY;
  l2cap_server.accept = l2cap_accept_cb;

  int err = bt_l2cap_server_register(&l2cap_server);
  if (err < 0) {
    LOG_INF("[ERROR] Failed to register L2CAP server (err %d)\n", err);
    return err;
  }

  return COC_ERR_OK;
}
#endif // CONFIG_BT_L2CAP_DYNAMIC_CHANNEL