

#include "ble_stack/gap/disc.h"


#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "ble_stack/gatt_db/gatt_client.h"
#include "ble_stack/gatt_db/gatt_db.h"
#include "ble_stack/util/uuids.h"
#include "peripherals/gpio.h"
#include "util/context.h"
#include "variables.h"
#if(IS_ENABLED(CONFIG_BT_GATT_DM))
#include <bluetooth/gatt_dm.h>
#define UUID_16_BASE_OFFSET 12

LOG_MODULE_REGISTER(ble_disc, LOG_LEVEL_DBG);

#if (BLE_DISCOVERY_MODE == BLE_DISCOVERY_MODE_CID)
bt_cid_client_t cid_client = {0};

/* Discovery helpers */
static uint8_t cid_notif_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data,
                            uint16_t length)
{
  if (!data)
  {
    LOG_DBG("[UNSUBSCRIBED]");
    return BT_GATT_ITER_STOP;
  }

  bt_cid_chr_rx_notification_cb(data, length);

  return BT_GATT_ITER_CONTINUE;
}
static int bt_cid_subscribe_receive(bt_cid_client_t *cid_client)
{
  int err;
  /** GATT subscribe parameters for NUS TX Characteristic. */
  cid_client->tx_notif_params.notify = cid_notif_cb;
  cid_client->tx_notif_params.value = BT_GATT_CCC_NOTIFY;
  cid_client->tx_notif_params.value_handle = cid_client->handles.tx;
  cid_client->tx_notif_params.ccc_handle = cid_client->handles.tx_ccc;

  err = bt_gatt_subscribe(cid_client->conn, &cid_client->tx_notif_params);
  if (err)
  {
    LOG_ERR("Subscribe failed (err %d)", err);
  }
  else
  {
    LOG_DBG("[SUBSCRIBED]");
  }
  return err;
}

static int bt_cid_handles_assign(struct bt_gatt_dm *dm, bt_cid_client_t *cid_client)
{
  const struct bt_gatt_dm_attr *gatt_service_attr = bt_gatt_dm_service_get(dm);
  const struct bt_gatt_service_val *gatt_service = bt_gatt_dm_attr_service_val(gatt_service_attr);
  const struct bt_gatt_dm_attr *gatt_chrc;
  const struct bt_gatt_dm_attr *gatt_desc;

  if (bt_uuid_cmp(gatt_service->uuid, BT_UUID_CIDS))
  {
    return -ENOTSUP;
  }
  LOG_DBG("Getting handles from CID service.");
  memset(&cid_client->handles, 0xFF, sizeof(cid_client->handles));

  /* CID Characteristic */
  gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_CIDS_INTAN_DATA);
  if (!gatt_chrc)
  {
    LOG_ERR("Missing CID characteristic.");
    return -EINVAL;
  }

  /* CID TX */
  gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_CIDS_INTAN_DATA);
  if (!gatt_desc)
  {
    LOG_ERR("Missing TX value descriptor in characteristic.");
    return -EINVAL;
  }
  LOG_DBG("Found handle for CID TX characteristic.");
  cid_client->handles.tx = gatt_desc->handle;
  /* CID TX CCC */
  gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_GATT_CCC);
  if (!gatt_desc)
  {
    LOG_ERR("Missing CID TX CCC in characteristic.");
    return -EINVAL;
  }
  LOG_DBG("Found handle for CCC of CID TX characteristic.");
  cid_client->handles.tx_ccc = gatt_desc->handle;

  /* Assign connection instance. */
  cid_client->conn = bt_gatt_dm_conn_get(dm);

  return 0;
}

static void discovery_complete(struct bt_gatt_dm *dm, void *context)
{
  bt_cid_client_t *cid_client = context;
  LOG_INF("Service discovery completed");

  bt_gatt_dm_data_print(dm);
  bt_cid_handles_assign(dm, cid_client);
  bt_cid_subscribe_receive(cid_client);
  bt_gatt_dm_data_release(dm);
}

static void discovery_service_not_found(struct bt_conn *conn, void *context)
{
  LOG_ERR("Service not found");
}

static void discovery_error(struct bt_conn *conn, int err, void *context)
{
  LOG_ERR("Error while discovering GATT database: (%d)", err);
}

struct bt_gatt_dm_cb discovery_cb = {
    .completed = discovery_complete,
    .service_not_found = discovery_service_not_found,
    .error_found = discovery_error,
};

void gatt_discover(struct bt_conn *conn)
{
  int err;
  /* Only discover on peripherals, so role must be central */
  conn_ctx_t *conn_ctx = get_conn_ctx_from_ref(conn);
  if (conn_ctx->id != BT_CONN_ROLE_CENTRAL)
  {
    LOG_WRN("Discovery procedure is aborted when the central is connecting to WCU");
    return;
  }

  cid_client.conn = conn;
  err = bt_gatt_dm_start(conn, BT_UUID_CIDS, &discovery_cb, &cid_client);
  if (err)
  {
    LOG_ERR(
        "could not start the discovery procedure, error "
        "code: %d",
        err);
  }
}

/* Dummy functions for gen clients */
discovered_chrc_t *bt_gen_get_discovered_chars(void)
{
  return NULL;
}
struct bt_conn *bt_gen_get_conn(void)
{
  return NULL;
}

#else
/* Discovery mode is not CID, so instead use generic handlers */
static bt_gen_client_t gen_client = {0};

/* Base UUID : 0000[0000]-0000-1000-8000-00805F9B34FB
 * 0x2800    : 0000[2800]-0000-1000-8000-00805F9B34FB
 *  little endian 0x2800 : [00 28] -> no swapping required
 *  big endian 0x2800    : [28 00] -> swapping required
 */
static const struct bt_uuid_128 uuid128_base = {
    .uuid = {BT_UUID_TYPE_128}, .val = {BT_UUID_128_ENCODE(0x00000000, 0x0000, 0x1000, 0x8000, 0x00805F9B34FB)}};

void uuid_to_uuid128(const struct bt_uuid *src, struct bt_uuid_128 *dst)
{
  switch (src->type)
  {
    case BT_UUID_TYPE_16:
      *dst = uuid128_base;
      sys_put_le16(BT_UUID_16(src)->val, &dst->val[UUID_16_BASE_OFFSET]);
      return;
    case BT_UUID_TYPE_32:
      *dst = uuid128_base;
      sys_put_le32(BT_UUID_32(src)->val, &dst->val[UUID_16_BASE_OFFSET]);
      return;
    case BT_UUID_TYPE_128:
      memcpy(dst, src, sizeof(*dst));
      return;
  }
}

/* Accesser functions to gen_client */
discovered_chrc_t *bt_gen_get_discovered_chars(void)
{
  return gen_client.discovered_chars;
}

struct bt_conn *bt_gen_get_conn(void)
{
  return gen_client.conn;
}

/* Exposed finder helpers */
discovered_chrc_t *bt_gen_find_discovered_char_by_uuid(const struct bt_uuid_128 *uuid_128)
{
  for (size_t i = 0; i < gen_client.discovered_count; ++i)
  {
    if (bt_uuid_cmp(&gen_client.discovered_chars[i].uuid_128.uuid, &uuid_128->uuid) == 0)
    {
      return &gen_client.discovered_chars[i];
    }
  }
  return NULL;
}

discovered_chrc_t *bt_gen_find_discovered_char_by_handle(uint16_t handle)
{
  for (size_t i = 0; i < gen_client.discovered_count; ++i)
  {
    if (gen_client.discovered_chars[i].val_handle == handle)
    {
      return &gen_client.discovered_chars[i];
    }
  }
  return NULL;
}

/* Notification callback */
uint8_t bt_gen_chr_tx_notification_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data,
                                      uint16_t length)
{
  if (!data)
  {
    LOG_DBG("[UNSUBSCRIBED]");
    return BT_GATT_ITER_STOP;
  }

  discovered_chrc_t *chrc = bt_gen_find_discovered_char_by_handle(params->value_handle);

  if (!chrc)
  {
    LOG_ERR("Received notification for unknown characteristic with handle 0x%04x", params->value_handle);
    return BT_GATT_ITER_CONTINUE;
  }

  bt_gen_client_char_req_t char_req = {
      .uuid_128 = chrc->uuid_128, .data = (void *)data, .len = length, .op = OP_NOTIFY};

  bt_gen_chr_rx_notification_cb(&char_req);

  return BT_GATT_ITER_CONTINUE;
}

/* Discovery helpers */
static void discovery_complete(struct bt_gatt_dm *dm, void *context)
{
  const struct bt_gatt_dm_attr *attr = NULL;
  bt_gatt_dm_data_print(dm);

  while ((attr = bt_gatt_dm_attr_next(dm, attr)) != NULL)
  {
    if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC) != 0)
    {
      continue;
    }
    struct bt_gatt_chrc *chrc = bt_gatt_dm_attr_chrc_val(attr);

    if (!chrc)
    {
      LOG_WRN("Could not find characteristic in attribute 0x%04x", attr->handle);
      continue;
    }

    if (gen_client.discovered_count >= MAX_DISCOVERED_CHARS)
    {
      LOG_WRN("Maximum discovered characteristics reached, skipping characteristic with handle 0x%04x",
              chrc->value_handle);
      continue;
    }

    char uuid_str[BT_UUID_STR_LEN];
    bt_uuid_to_str(chrc->uuid, uuid_str, sizeof(uuid_str));
    LOG_DBG("Discovered characteristic, uuid: %s", uuid_str);
    const struct bt_gatt_dm_attr *value_attr = bt_gatt_dm_attr_by_handle(dm, chrc->value_handle);
    if (!value_attr)
    {
      LOG_WRN("Could not find value attr for handle 0x%04x", chrc->value_handle);
      continue;
    }

    /* Convert uuid to 128 always for rubustness */
    uuid_to_uuid128(chrc->uuid, &gen_client.discovered_chars[gen_client.discovered_count].uuid_128);

    // Fill the discovered_chars array
    gen_client.discovered_chars[gen_client.discovered_count].val_handle = chrc->value_handle;
    gen_client.discovered_chars[gen_client.discovered_count].perm = value_attr->perm;

    gen_client.discovered_count++;

    /* Subscribe if possible */
    if (gen_client.subscribed_count >= MAX_SUBSCRIBED_CHARS)
    {
      LOG_WRN("Maximum subscribed characteristics reached, skipping subscription for handle 0x%04x",
              chrc->value_handle);
      continue;
    }

    if (chrc->properties & BT_GATT_CHRC_NOTIFY || chrc->properties & BT_GATT_CHRC_INDICATE)
    {
      struct bt_gatt_subscribe_params *params = &gen_client.tx_notif_params[gen_client.subscribed_count];
      params->notify = bt_gen_chr_tx_notification_cb;
      params->value_handle = chrc->value_handle;
      const struct bt_gatt_dm_attr *desc = bt_gatt_dm_desc_by_uuid(dm, attr, BT_UUID_GATT_CCC);
      params->ccc_handle = desc->handle;
      params->value = BT_GATT_CCC_NOTIFY;

      LOG_DBG("Attempting to subscribe to handle 0x%04x, desc:0x%04x", chrc->value_handle, desc->handle);
      int err = bt_gatt_subscribe(gen_client.conn, params);
      if (err)
      {
        LOG_ERR("Failed to subscribe to characteristic 0x%04x: %d", chrc->value_handle, err);
      }
      else
      {
        LOG_DBG("Subscribed to characteristic 0x%04x", chrc->value_handle);
        gen_client.subscribed_count++;
      }
    }
  }

  bt_gatt_dm_data_release(dm);
  bt_gatt_dm_continue(dm, &gen_client);
}

static void discovery_service_not_found(struct bt_conn *conn, void *context)
{
  LOG_ERR("Service not found");
}

static void discovery_error(struct bt_conn *conn, int err, void *context)
{
  LOG_ERR("Error while discovering GATT database: (%d)", err);
}

struct bt_gatt_dm_cb discovery_cb = {
    .completed = discovery_complete,
    .service_not_found = discovery_service_not_found,
    .error_found = discovery_error,
};

void gatt_discover(struct bt_conn *conn)
{
  int err;
  /* Only discover on peripherals, so role must be central */
  conn_ctx_t *conn_ctx = get_conn_ctx_from_ref(conn);
  if (conn_ctx->id != BT_CONN_ROLE_CENTRAL)
  {
    LOG_WRN("Discovery procedure is aborted when the central is connecting to WCU");
    return;
  }

  /* Initialize client */
  gen_client.conn = conn;
  gen_client.discovered_count = 0;
  gen_client.subscribed_count = 0;
  memset(gen_client.discovered_chars, 0, sizeof(gen_client.discovered_chars));
  memset(gen_client.tx_notif_params, 0, sizeof(gen_client.tx_notif_params));

  err = bt_gatt_dm_start(conn,
                         NULL,  // Use NULL to discover all services
                         &discovery_cb, &gen_client);
  if (err)
  {
    LOG_ERR(
        "could not start the discovery procedure, error "
        "code: %d",
        err);
  }
}

#endif

#endif // (IS_ENABLED(CONFIG_BT_GATT_DM))