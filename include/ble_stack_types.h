#ifndef BLE_WRAPPER_TYPES_H
#define BLE_WRAPPER_TYPES_H
#include <stdint.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#define UNUSED(x) (void)(x)
#define MAX_DISCOVERED_CHARS 30
#define MAX_SUBSCRIBED_CHARS 10

typedef uint8_t (*bt_gatt_handler_t)(const void *, uint16_t);
typedef uint8_t (*gen_rx_notif_cb_func)(const struct bt_uuid_128 *, const void *, const uint16_t, const uint8_t);
typedef enum
{
  GATT_DB_SVC_CTS_CHAR_CURR_TIME,
  GATT_DB_SVC_LN_CHAR_FEAT,
  GATT_DB_SVC_LN_CHAR_GPS,
  GATT_DB_SVC_LN_CHAR_PQ,
  GATT_DB_MAX_ENTRIES,
} gatt_db_characteristics_t;

typedef enum
{
  OP_READ,
  OP_WRITE,
  OP_WRITE_WITHOUT_RESP,
  OP_NOTIFY,
} op_type_t;

typedef struct
{
  struct bt_uuid_128 uuid_128; /**< 128 bit form UUID of the service */
  uint16_t val_handle;
  uint8_t perm;
} discovered_chrc_t;

typedef struct
{
  void *fifo_reserved;         // For internal use by Zephyr's FIFO implementation
  struct bt_uuid_128 uuid_128; /**< UUID of the characteristic */
  void *data;                  /**< Pointer to the data buffer for the characteristic */
  uint16_t len;                /**< Length of the data buffer */
  uint8_t op;                  /* Operation type @ref op_type_t */
} bt_gen_client_char_req_t;

#if defined(CONFIG_BT_GATT_CLIENT)
typedef struct
{
  /** Connection object. */
  struct bt_conn *conn;
  /** Handles on the connected peer device that are needed
   * to interact with the device.
   */
  discovered_chrc_t discovered_chars[MAX_DISCOVERED_CHARS];
  /** GATT subscribe parameters for NUS TX Characteristic. */
  struct bt_gatt_subscribe_params tx_notif_params[MAX_SUBSCRIBED_CHARS];
  uint16_t discovered_count;
  uint16_t subscribed_count;
} bt_gen_client_t;
#endif // CONFIG_BT_GATT_CLIENT

#endif /* BLE_WRAPPER_TYPES_H */