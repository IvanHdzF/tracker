#include "ble_stack/gatt_db/gatt_client.h"

#include <stdint.h>
#include <string.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_stack/gap/disc.h"
#include "ble_stack/util/uuids.h"
#include "ble_stack_types.h"
#include "util/mem_debug.h"
#include "util/mutex.h"
#include "variables.h"

#define GATT_REQ_HANDLER_STACK_DEPTH 4096
#define GATT_REQ_HANDLER_PRIORITY 6
#define GATT_CLIENT_RW_TIMEOUT_MS 100

/* Entry for characteristics that we are reading from as clients */
gen_rx_notif_cb_func gatt_client_gen_rx_cb = NULL;  // Generic service callback, can be used for any service

struct bt_gatt_read_params gatt_client_read_params = {0};
struct bt_gatt_write_params gatt_client_write_params = {0};

typedef enum
{
  GATT_CLIENT_SUCCESS = 0,
  GATT_CLIENT_ERR_NO_MEM = -1,
  GATT_CLIENT_INVALID_ATTR = -2
} gatt_client_error_codes_t;

LOG_MODULE_REGISTER(gatt_client, LOG_LEVEL_DBG);

K_FIFO_DEFINE(fifo_gatt_client_notify_handler);

/* Helper functions */
/* Notifications */

static void call_generic_rx_cb(bt_gen_client_char_req_t* request)
{
  if (gatt_client_gen_rx_cb != NULL)
  {
    gatt_client_gen_rx_cb(&request->uuid_128, request->data, request->len, request->op);
  }
  else
  {
    LOG_DBG("Generic RX callback not set, skipping");
  }
}

uint8_t gen_notify_rx_handler(bt_gen_client_char_req_t* request)
{
  if (request == NULL || request->data == NULL)
  {
    LOG_WRN("Received NULL request or data, skipping");
    return GATT_CLIENT_ERR_NO_MEM;
  }

  char uuid_str[BT_UUID_STR_LEN];
  bt_uuid_to_str(&request->uuid_128.uuid, uuid_str, sizeof(uuid_str));
  LOG_DBG("Received data for characteristic with UUID %s, data length: %d", uuid_str, request->len);

  // TODO: Question, should this layer be the one in charge of routing the handling or do we leave it as -is and let the app layer 
  // handle it?

  // TODO: Consider handling operation type in detail here by adding new types
  // For now, we are treating all operations the same in the generic callback, 
  // but in the future we can add more specific handling based on the operation type (e.g., OP_READ, OP_WRITE, OP_NOTIFY)
  // Or based on specific characteristics if needed by adding more fields to the request struct

  /* Apply generic callback */
  call_generic_rx_cb(request);

  return GATT_CLIENT_SUCCESS;
}

/* Request helpers */
static bt_gen_client_char_req_t* allocate_client_request_data(uint16_t req_data_len)
{
  bt_gen_client_char_req_t* request = k_malloc_safe(sizeof(bt_gen_client_char_req_t));
  if (request == NULL)
  {
    LOG_WRN("Could not allocate memory for request");
    return NULL;
  }

  request->data = k_malloc_safe(req_data_len);
  if (request->data == NULL)
  {
    LOG_WRN("Could not allocate memory for request data");
    k_free_safe(request);
    return NULL;
  }

  // Initialize all fields to safe defaults
  request->uuid_128 = (struct bt_uuid_128){0};  // Initialize UUID to zero
  request->len = req_data_len;
  return request;
}

static void gatt_client_create_request(bt_gen_client_char_req_t* in_request)
{
  bt_gen_client_char_req_t* request = NULL;
  request = allocate_client_request_data(in_request->len);
  if (request == NULL)
  {
    LOG_WRN("Could not allocate memory for request");
    return;
  }

  /* Fill the request */
  memcpy(&request->uuid_128, &in_request->uuid_128, sizeof(struct bt_uuid_128));  // Copy the UUID
  memcpy(request->data, in_request->data, in_request->len);                       // Copy the data
  request->len = in_request->len;
  request->op = in_request->op;

  k_fifo_put(&fifo_gatt_client_notify_handler, request);
}

void bt_gen_chr_rx_notification_cb(bt_gen_client_char_req_t* client_req)
{
  gatt_client_create_request(client_req);
}

/* R/W Operations */
uint8_t gatt_client_read_cb(struct bt_conn* conn, uint8_t err, struct bt_gatt_read_params* params, const void* data,
                            uint16_t length)
{
  if (err != 0)
  {
    LOG_ERR("GATT Client Read Error: %d", err);
  }
  if (data == NULL || length == 0)
  {
    LOG_WRN("Received NULL data or zero length in read callback");
    return BT_GATT_ERR(GATT_CLIENT_ERR_NO_MEM);
  }
  LOG_DBG("GATT Client Read Callback: Data length: %d", length);

  /* Find characteristic in array */
  discovered_chrc_t* chrc = bt_gen_find_discovered_char_by_handle(params->single.handle);

  if (!chrc)
  {
    LOG_ERR("Finished a read procedure with invalid handle %d", params->single.handle);
    return BT_GATT_ITER_CONTINUE;
  }

  /* Create request to work handler */
  bt_gen_client_char_req_t read_req = {
      .data = (void*)data,
      .len = length,
      .uuid_128 = chrc->uuid_128,
      .op = OP_READ,
  };
  gatt_client_create_request(&read_req);

  /* Buffer has been copied into request, so we can safely give semaphore to the worker thread to do
   * k_free */
  sem_gatt_client_rw_give();

  return BT_GATT_ITER_CONTINUE;
}

static int gatt_client_read_chrc(discovered_chrc_t* characteristic_entry)
{
  int err = 0;
  /* Get connection */
  struct bt_conn* client_conn = bt_gen_get_conn();

  /* Prepare read parameters */
  gatt_client_read_params.func = gatt_client_read_cb;
  gatt_client_read_params.handle_count = 1;
  gatt_client_read_params.single.handle = characteristic_entry->val_handle;
  gatt_client_read_params.single.offset = 0;

  /* Take a semaphore to synchronize k_free */
  err = sem_gatt_client_rw_take(K_MSEC(GATT_CLIENT_RW_TIMEOUT_MS));
  if (err)
  {
    LOG_ERR("Failure to obtain semapahore! This should not be happening, aborting request.");
    return err;
  }

  err = bt_gatt_read(client_conn, &gatt_client_read_params);

  return err;
}

void gatt_client_write_cb(struct bt_conn* conn, uint8_t err, struct bt_gatt_write_params* params)
{
  /* Find characteristic in array */
  discovered_chrc_t* chrc = bt_gen_find_discovered_char_by_handle(params->handle);

  if (!chrc)
  {
    LOG_ERR("Finished a write procedure with invalid handle %d", params->handle);
    return;
  }

  /* Write responses only need a response code */
  uint8_t data = err;
  uint16_t len = sizeof(data);

  /* Create request to work handler */
  bt_gen_client_char_req_t write_req = {
      .data = &data,
      .len = len,
      .uuid_128 = chrc->uuid_128,
      .op = OP_WRITE,
  };
  gatt_client_create_request(&write_req);
  /* Buffer has been copied into request, so we can safely give semaphore to the worker thread to do
   * k_free */
  sem_gatt_client_rw_give();
}

static int gatt_client_write_chrc(discovered_chrc_t* characteristic_entry, const void* data, const uint16_t len)
{
  int err = 0;
  if (data == NULL)
  {
    LOG_WRN("Write operation failed because there is no data to write!");
    return BT_GATT_ERR(GATT_CLIENT_ERR_NO_MEM);
  }

  /* Get connection */
  struct bt_conn* client_conn = bt_gen_get_conn();

  /* Prepare write parameters */
  gatt_client_write_params.func = gatt_client_write_cb;
  gatt_client_write_params.handle = characteristic_entry->val_handle;
  gatt_client_write_params.offset = 0;
  gatt_client_write_params.data = data;
  gatt_client_write_params.length = len;

  /* Take a semaphore to synchronize k_free */
  err = sem_gatt_client_rw_take(K_MSEC(GATT_CLIENT_RW_TIMEOUT_MS));
  if (err)
  {
    LOG_ERR("Failure to obtain semapahore! This should not be happening, aborting request.");
    return err;
  }

  err = bt_gatt_write(client_conn, &gatt_client_write_params);

  return err;
}

static int gatt_client_write_no_resp_chrc(discovered_chrc_t* characteristic_entry, const void* data, const uint16_t len)
{
  if (data == NULL)
  {
    LOG_WRN("Write operation failed because there is no data to write!");
    return BT_GATT_ERR(GATT_CLIENT_ERR_NO_MEM);
  }

  /* Get connection */
  struct bt_conn* client_conn = bt_gen_get_conn();

  return bt_gatt_write_without_response(client_conn, characteristic_entry->val_handle, data, len, false);
}

/* Worker thread */
void gatt_client_notify_handler(void* p1, void* p2, void* p3)
{
  UNUSED(p1);
  UNUSED(p2);
  UNUSED(p3);
  uint8_t err = 0;

  for (;;)
  {
    bt_gen_client_char_req_t* request = k_fifo_get(&fifo_gatt_client_notify_handler, K_FOREVER);
    if (request == NULL)
    {
      LOG_WRN("Received NULL request, skipping");
      continue;
    }

    mutex_worker_handler_lock();
    err = gen_notify_rx_handler(request);
    mutex_worker_handler_unlock();

    if (err != GATT_CLIENT_SUCCESS)
    {
      LOG_WRN("Error handling notification, error code: %d", err);
    }

    /* Check for semaphore to guarantee that there are no pending R/W operations */
    err = sem_gatt_client_rw_take(K_MSEC(GATT_CLIENT_RW_TIMEOUT_MS));
    if (err)
    {
      LOG_ERR("Failure to obtain semaphore before k_free, unintended behaviors can occur!.");
    }
    else
    {
      /* Return semaphore, everything is good as expected */
      sem_gatt_client_rw_give();
    }

    if (request->data != NULL)
    {
      k_free_safe(request->data);
      request->data = NULL;
    }
    k_free_safe(request);
  }
}

/* Exported functions */

/* Notifications */
ssize_t bt_gen_chr_set_gen_rx_cb(gen_rx_notif_cb_func gen_rx_cb)
{
  /* Set the Generic RX callback */
  gatt_client_gen_rx_cb = gen_rx_cb;
  return BT_ATT_ERR_SUCCESS;
}

/* Read / Write attributes */
int bt_client_gen_chrc_do_op(const struct bt_uuid_128* uuid_128, uint8_t op_type, const void* data, const uint16_t len)
{
  int err = 0;
  /* Get the characteristic handle */
  discovered_chrc_t* discovered_characteristics = bt_gen_get_discovered_chars();
  if (discovered_characteristics == NULL)
  {
    LOG_ERR("No discovered characteristics available");
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
  }

  /* Find the characteristic handle by UUID */
  discovered_chrc_t* characteristic_entry = NULL;
  characteristic_entry = bt_gen_find_discovered_char_by_uuid(uuid_128);

  if (characteristic_entry == NULL)
  {
    /* Characteristic not found */
    char uuid_str[BT_UUID_STR_LEN];
    bt_uuid_to_str(&uuid_128->uuid, uuid_str, sizeof(uuid_str));
    LOG_ERR("Characteristic with UUID %s not found", uuid_str);
    return BT_GATT_ERR(BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
  }

  /* Handle operation type as needed */
  switch (op_type)
  {
    case OP_READ:
    {
      err = gatt_client_read_chrc(characteristic_entry);
      break;
    }
    case OP_WRITE:
    {
      err = gatt_client_write_chrc(characteristic_entry, data, len);
      break;
    }
    case OP_WRITE_WITHOUT_RESP:
    {
      err = gatt_client_write_no_resp_chrc(characteristic_entry, data, len);
      break;
    }

    default:
    {
      err = BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
      LOG_WRN("Invalid operation type on request, request dropped");
    }
  }

  return err;
}

K_THREAD_DEFINE(gatt_client_notify_handler_thread_id, GATT_REQ_HANDLER_STACK_DEPTH, gatt_client_notify_handler, NULL,
                NULL, NULL, GATT_REQ_HANDLER_PRIORITY, 0, 0);