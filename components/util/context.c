#include "util/context.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_wrapper_ctx, LOG_LEVEL_DBG);

/* Connection context */
static conn_ctx_t conn_ctx[MAX_CONN] = {0};

conn_ctx_t *get_conn_ctx_available(void)
{
  for (uint8_t i = 0; i < MAX_CONN; i++)
  {
    if (conn_ctx[i].conn_status == CONN_STATUS_DISCONNECTED)
    {
      conn_ctx[i].id = i;
      return &conn_ctx[i];
    }
  }

  return NULL;
}

conn_ctx_t *get_conn_ctx_from_ref(struct bt_conn *conn)
{
  LOG_DBG("Looking for connection context with reference %p", (void *)conn);
  for (uint8_t i = 0; i < MAX_CONN; i++)
  {
    LOG_DBG("Checking conn_ctx[%d] with status %d and conn %p", i, conn_ctx[i].conn_status, (void *)conn_ctx[i].conn);
    if (conn_ctx[i].conn == conn)
    {
      return &conn_ctx[i];
    }
  }

  LOG_WRN("No connection context with passed reference");
  return NULL;
}

conn_ctx_t *get_conn_ctx_from_id(uint8_t id)
{
  if (id < MAX_CONN)
  {
    return &conn_ctx[id];
  }

  return NULL;
}
