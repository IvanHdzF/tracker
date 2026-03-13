#include "ble_stack/gatt_db/gatt_db.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "ble_stack/gatt_db/service_cts.h"
#include "ble_stack/util/uuids.h"
#include "peripherals/gpio.h"
#include "ble_stack_types.h"
#include "util/context.h"
#include "util/mem_debug.h"
#include "util/mutex.h"
#include "variables.h"

#define GATT_REQ_HANDLER_STACK_DEPTH 4096
#define GATT_REQ_HANDLER_PRIORITY 6

/* Zephyr helper declarations */
LOG_MODULE_REGISTER(ble_gatt_db, LOG_LEVEL_DBG);
K_FIFO_DEFINE(fifo_gatt_char_work_handler);

#if (DEBUG == 1)
uint32_t packets_dropped = 0; /* Counter for dropped packets */
#endif


/* Characteristic values */
static uint64_t current_time_value = {0};
static const uint32_t ln_feat = LOC_AND_SPEED_DATA_FLAGS_MASK;
static loc_and_speed_data_t location_and_speed_data = {
  .flags = LOC_AND_SPEED_DATA_FLAGS_MASK,
};

static pos_qual_t pos_qual_data = {
  .flags = POSITION_QUALITY_FLAGS_MASK,
};

/* As server */
/* Struct with all the characteristics */

// TODO: GATT DB is highly coupled to wrapper requests, consider refactor to decouple and make more modular for future features and easier testing
bt_char_entry_t gatt_char_db[GATT_DB_MAX_ENTRIES] = {
    /* Current Time Service */
    [GATT_DB_SVC_CTS_CHAR_CURR_TIME] =
        {
            .data = &current_time_value,
            .length = sizeof(current_time_value),
            .read_cb = NULL,
            .write_cb = NULL,
            .notify_cb = NULL,
        },
    /* Location and Navigation Service */
    [GATT_DB_SVC_LN_CHAR_FEAT] =
        {
            .data = (void*)&ln_feat,
            .length = sizeof(ln_feat),
            .read_cb = NULL,
            .write_cb = NULL,
            .notify_cb = NULL,
        },
    [GATT_DB_SVC_LN_CHAR_GPS] =
        {
            .data = &location_and_speed_data,
            .length = sizeof(location_and_speed_data),
            .read_cb = NULL,
            .write_cb = NULL,
            .notify_cb = NULL,
        },
    [GATT_DB_SVC_LN_CHAR_PQ] =
        {
            .data = &pos_qual_data,  // Using same struct for simplicity, can be changed to a different struct if needed
            .length = sizeof(pos_qual_data),
            .read_cb = NULL,
            .write_cb = NULL,
            .notify_cb = NULL,
        },
};

/* Helper functions */
static void get_op_handler(bt_char_entry_t *gatt_entry, uint8_t op, bt_gatt_handler_t *op_handler)
{
  switch (op)
  {
    case OP_READ:
    {
      *op_handler = gatt_entry->read_cb;
      break;
    }
    case OP_WRITE:
    {
      *op_handler = gatt_entry->write_cb;
      break;
    }
    case OP_NOTIFY:
    {
      // TODO: OP_NOTIFY is not being used at all, consider dropping if it does not make sense
      *op_handler = gatt_entry->notify_cb;
      break;
    }
    default:
    {
      LOG_WRN("Invalid operation type on request, request dropped");
    }
  }
  return;
}

static bt_char_work_t *allocate_request_data(uint16_t req_data_len)
{
  bt_char_work_t *request = k_malloc_safe(sizeof(bt_char_work_t));
  if (request == NULL)
  {
    LOG_WRN("Could not allocate memory for request");
    return NULL;
  }

  request->req_data = k_malloc_safe(req_data_len);
  if (request->req_data == NULL)
  {
    LOG_WRN("Could not allocate memory for request data");
    k_free_safe(request);
    return NULL;
  }

  // Initialize all fields to safe defaults
  request->entry = NULL;
  request->op = 0xFF;  // Invalid operation type
  request->req_data_len = req_data_len;
  return request;
}

/* Thread function, handles BLE packets received across GATT */
void gatt_char_work_handler(void *p1, void *p2, void *p3)
{
  UNUSED(p1);
  UNUSED(p2);
  UNUSED(p3);

  for (;;)
  {
    bt_char_work_t *request = k_fifo_get(&fifo_gatt_char_work_handler, K_FOREVER);
    bt_char_entry_t *gatt_entry = request->entry;
    if (gatt_entry == NULL)
    {
      LOG_WRN("Request entry is NULL, skipping request");
      if (request->req_data != NULL)
      {
        k_free_safe(request->req_data);
        request->req_data = NULL;
      }
      k_free_safe(request);
#if (DEBUG == 1)
      packets_dropped++;
#endif
      continue;  // No entry to process, skip this request
    }

    /* By design the handler expects data */
    if (request->req_data == NULL)
    {
      k_free_safe(request);
      LOG_WRN("Request data is NULL, skipping request");
#if (DEBUG == 1)
      packets_dropped++;
#endif
      continue;  // No data to process, skip this request
    }

    bt_gatt_handler_t op_handler = NULL;
    get_op_handler(gatt_entry, request->op, &op_handler);

    /* Redundant check for future features */
    if (op_handler == NULL)
    {
      LOG_WRN("Packet dropped due to not having an operation initialized");
#if (DEBUG == 1)
      packets_dropped++;
#endif
      continue;
    }

    /* Process request as needed */
    mutex_worker_handler_lock();
    op_handler(request->req_data, request->req_data_len);
    // TODO: Consider either handling error of callback here, OR directly removing uint8_t type out of the signature
    mutex_worker_handler_unlock();

    if (request->req_data != NULL)
    {
      k_free_safe(request->req_data);
      request->req_data = NULL;
    }
    k_free_safe(request);
    LOG_DBG("Request processed and freed");
  }
}

/* CID Char Helper function */

/* Generic Handlers */

ssize_t bt_gen_chr_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
  bt_char_entry_t *characteristic_entry = (bt_char_entry_t *)attr->user_data;

  if (characteristic_entry == NULL)
  {
    LOG_WRN("Characteristic entry is NULL, skipping read request");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  if (offset > characteristic_entry->length)
  {
    LOG_WRN("Offset %u out of bounds (char_len = %u)", offset, characteristic_entry->length);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }

  size_t available = characteristic_entry->length - offset;
  size_t to_copy = MIN(len, available);

  if (characteristic_entry->read_cb && to_copy > 0)
  {
    bt_char_work_t *request = allocate_request_data(to_copy);
    if (request == NULL)
    {
      LOG_WRN("Could not allocate memory for request");
      return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }

    request->entry = characteristic_entry;
    request->op = OP_READ;
    memcpy(request->req_data, ((uint8_t *)characteristic_entry->data) + offset, to_copy);
    request->req_data_len = to_copy;

    LOG_DBG("bt_gen_chr_read: request->entry = %p", request->entry);
    k_fifo_put(&fifo_gatt_char_work_handler, request);
  }

  return bt_gatt_attr_read(conn, attr, buf, len, offset, characteristic_entry->data, characteristic_entry->length);
}

ssize_t bt_gen_chr_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                         uint16_t offset, uint8_t flags)
{
  /* Get the characteristic entry */
  bt_char_entry_t *characteristic_entry = (bt_char_entry_t *)attr->user_data;

  if (characteristic_entry == NULL)
  {
    LOG_WRN("Characteristic entry is NULL, skipping write request");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  if (len > characteristic_entry->length)
  {
    LOG_WRN("Write Request not processed due to invalid lengths:\nlen = %d\nchar_len = %d", len,
            characteristic_entry->length);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  if (characteristic_entry->write_cb)
  {
    bt_char_work_t *request = allocate_request_data(len);
    if (request == NULL)
    {
      LOG_WRN("Could not allocate memory for request");
      return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
    }
    /* Fill the request */
    request->entry = characteristic_entry;
    request->op = OP_WRITE;
    memcpy(request->req_data, buf, len);  // Copy the data to the request data
    request->req_data_len = len;          // Set the length of the request data
    LOG_DBG("bt_gen_chr_write: request->entry = %p", request->entry);
    k_fifo_put(&fifo_gatt_char_work_handler, request);
  }

  memcpy(characteristic_entry->data, buf, len);
  return len;
}

/* Accesser functions for USER API */
/* Set user callbacks to gatt entries (As a server) */
ssize_t bt_get_attr_value_and_len(const struct bt_uuid *uuid, void *value, uint16_t *len)
{
  /* Get the characteristic handle */
  struct bt_gatt_attr *attr = bt_gatt_find_by_uuid(NULL, 1, uuid);
  if (attr == NULL)
  {
    char uuid_str[BT_UUID_STR_LEN];  // UUIDs are 36 characters + null terminator
    bt_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));
    LOG_WRN("Characteristic with UUID %s not found", uuid_str);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  /* Get the characteristic entry */
  bt_char_entry_t *characteristic_entry = (bt_char_entry_t *)attr->user_data;

  if (characteristic_entry == NULL)
  {
    LOG_WRN("Characteristic entry is NULL, skipping read request");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  if (value == NULL || len == NULL)
  {
    LOG_WRN("Value or length pointer is NULL");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
  }

  if (*len < characteristic_entry->length)
  {
    LOG_WRN("Provided buffer length (%d) is smaller than characteristic length (%d)", *len,
            characteristic_entry->length);
    return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
  }

  memcpy(value, characteristic_entry->data, characteristic_entry->length);
  *len = characteristic_entry->length;
  return BT_ATT_ERR_SUCCESS;
}

ssize_t bt_gen_chr_set_read_cb(const struct bt_uuid *uuid, bt_gatt_handler_t read_cb)
{
  /* Get the characteristic handle */
  struct bt_gatt_attr *attr = bt_gatt_find_by_uuid(NULL, 1, uuid);
  if (attr == NULL)
  {
    char uuid_str[BT_UUID_STR_LEN];  // UUIDs are 36 characters + null terminator
    bt_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));
    LOG_WRN("Characteristic with UUID %s not found", uuid_str);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  /* Get the characteristic entry */
  bt_char_entry_t *characteristic_entry = (bt_char_entry_t *)attr->user_data;

  if (characteristic_entry == NULL)
  {
    LOG_WRN("Characteristic entry is NULL, skipping read callback set");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  characteristic_entry->read_cb = read_cb;
  return BT_ATT_ERR_SUCCESS;
}

ssize_t bt_gen_chr_set_write_cb(const struct bt_uuid *uuid, bt_gatt_handler_t write_cb)
{
  /* Get the characteristic handle */
  struct bt_gatt_attr *attr = bt_gatt_find_by_uuid(NULL, 1, uuid);
  if (attr == NULL)
  {
    char uuid_str[BT_UUID_STR_LEN];  // UUIDs are 36 characters + null terminator
    bt_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));
    LOG_WRN("Characteristic with UUID %s not found", uuid_str);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  /* Get the characteristic entry */
  bt_char_entry_t *characteristic_entry = (bt_char_entry_t *)attr->user_data;

  if (characteristic_entry == NULL)
  {
    LOG_WRN("Characteristic entry is NULL, skipping write callback set");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  characteristic_entry->write_cb = write_cb;
  return BT_ATT_ERR_SUCCESS;
}

ssize_t bt_gen_chr_set_notify_tx_cb(const struct bt_uuid *uuid, bt_gatt_handler_t notify_cb)
{
  /* Get the characteristic handle */
  struct bt_gatt_attr *attr = bt_gatt_find_by_uuid(NULL, 1, uuid);
  if (attr == NULL)
  {
    char uuid_str[BT_UUID_STR_LEN];  // UUIDs are 36 characters + null terminator
    bt_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));
    LOG_WRN("Characteristic with UUID %s not found", uuid_str);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  /* Get the characteristic entry */
  bt_char_entry_t *characteristic_entry = (bt_char_entry_t *)attr->user_data;

  if (characteristic_entry == NULL)
  {
    LOG_WRN("Characteristic entry is NULL, skipping notify TX callback set");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  characteristic_entry->notify_cb = notify_cb;
  return BT_ATT_ERR_SUCCESS;
}

ssize_t bt_gen_chr_notify_tx(const struct bt_uuid *uuid, const void *data, const uint16_t len)
{
  /* Get the characteristic handle */
  struct bt_gatt_attr *attr;
  const struct bt_gatt_chrc *chrc;
  struct bt_gatt_attr *target = NULL;

  // Get both bt_gatt_chrc (char metadata) and target (char value)
  for (attr = bt_gatt_attr_next(NULL); attr; attr = bt_gatt_attr_next(attr)) {

      if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC) != 0) {
          continue;
      }

      chrc = attr->user_data;

      if (bt_uuid_cmp(chrc->uuid, uuid) == 0) {
          target = bt_gatt_attr_next(attr);
          break;
      }
  }
  if (target == NULL)
  {
    char uuid_str[BT_UUID_STR_LEN];  // UUIDs are 36 characters + null terminator
    bt_uuid_to_str(uuid, uuid_str, sizeof(uuid_str));
    LOG_WRN("Characteristic with UUID %s not found", uuid_str);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  bt_char_entry_t *characteristic_entry = (bt_char_entry_t *)target->user_data;

  if (characteristic_entry == NULL)
  {
    LOG_WRN("Characteristic entry is NULL, skipping notify request");
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE);
  }

  if (len > characteristic_entry->length)
  {
    LOG_WRN("Notify Request not processed due to invalid lengths:\nlen = %d\nchar_len = %d", len,
            characteristic_entry->length);
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  /* Update the characteristic for future reads if needed */
  memcpy(characteristic_entry->data, data, len);

  /* Check for notify property */

  if ((chrc->properties & BT_GATT_CHRC_NOTIFY) == 0)
  {
    LOG_WRN("No notify property! But memory was copied");
    return 0;
  }

  /* Iterate over possible connections to notify them */
  uint8_t notified = 0;
  for (uint8_t id = 0; id < MAX_CONN; id++)
  {
    /* If any connection is subscribed, send a broadcast and continue */
    conn_ctx_t *conn_ctx = get_conn_ctx_from_id(id);
    if (conn_ctx->conn == NULL)
    {
      LOG_DBG("NULL Conn");
      continue;
    }

    /* Current scheme uses notification but can be easily changed to indication to guarantee replies
     */
    if (bt_gatt_is_subscribed(conn_ctx->conn, target, BT_GATT_CCC_NOTIFY))
    {
      LOG_DBG("Notifying connection with id = %d", id);
      int err = bt_gatt_notify(conn_ctx->conn, target, data, len);
      if (err != 0)
      {
        LOG_ERR("Error %d while attempting to notify conn_id %d ", err, id);
      }
      notified++;
    }
    // TODO: Add support for indications if needed
  }

  LOG_DBG("bt_gen_chr_notify_tx: Notified %d connections", notified);

  /* Returns BT_ATT_ERR_CCC_IMPROPER_CONF if no device is subscribed */
  return (notified > 0) ? BT_ATT_ERR_SUCCESS : BT_GATT_ERR(BT_ATT_ERR_CCC_IMPROPER_CONF);
}

K_THREAD_DEFINE(gatt_char_work_handler_thread_id, GATT_REQ_HANDLER_STACK_DEPTH, gatt_char_work_handler, NULL, NULL,
                NULL, GATT_REQ_HANDLER_PRIORITY, 0, 0);