#ifndef CONTEXT_H
#define CONTEXT_H

#include <zephyr/kernel.h>

/* Connection context */
#define MAX_CONN CONFIG_BT_MAX_CONN

typedef enum
{
  CONN_STATUS_DISCONNECTED,
  CONN_STATUS_CONNECTED,
} conn_status_t;

typedef struct
{
  uint8_t id;                /* Connection application identifier */
  conn_status_t conn_status; /* Connection status */
  uint16_t conn_handle;      /* Connection BLE stack handler */
  struct bt_conn *conn;      /* Connection BLE stack context */
  struct
  {
    uint8_t gpio_pin; /* Connection pin status GPIO */
  } data;
} conn_ctx_t;

conn_ctx_t *get_conn_ctx_available(void);
conn_ctx_t *get_conn_ctx_from_ref(struct bt_conn *conn);
conn_ctx_t *get_conn_ctx_from_id(uint8_t id);

#endif /* CONTEXT_H */