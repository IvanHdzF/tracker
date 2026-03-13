#ifndef GATT_DB_H
#define GATT_DB_H

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include "ble_stack_types.h"

/* Permission macro */
#if (CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION == n)
#define GATT_DB_PERM_READ BT_GATT_PERM_READ
#define GATT_DB_PERM_WRITE BT_GATT_PERM_WRITE
#else
#define GATT_DB_PERM_READ BT_GATT_PERM_READ_AUTHEN
#define GATT_DB_PERM_WRITE BT_GATT_PERM_WRITE_AUTHEN
#endif

typedef struct
{
  void *data;
  uint16_t length;
  bt_gatt_handler_t read_cb;
  bt_gatt_handler_t write_cb;
  bt_gatt_handler_t notify_cb;
} bt_char_entry_t;

typedef struct
{
  void *fifo_reserved;  // For internal use by Zephyr's FIFO implementation
  bt_char_entry_t *entry;
  uint8_t op;
  void *req_data;         // Data associated with the request
  uint16_t req_data_len;  // Length of the request data
} bt_char_work_t;

/**
 * @brief Generic characteristic read handler
 *
 * Characteristic read generic callback, reads attribute based on user data which is intended to be
 * of type bt_char_entry_t
 * @param conn Connection handle
 * @param attr Attribute handle
 * @param buf Buffer to store the data
 * @param len Length of data to be read into the buffer
 * @param offset Offset from which to read the data (For partial reads)
 * @return ssize_t
 */
ssize_t bt_gen_chr_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len,
                        uint16_t offset);

/**
 * @brief Generic characteristic write handler
 *
 * @param conn Connection handle
 * @param attr Attribute handle
 * @param buf Buffer that contains data to be written
 * @param len Length of data to be written
 * @param offset Offset from which to write the data (For partial writes)
 * @param flags Unused in this implementation
 * @return ssize_t
 */
ssize_t bt_gen_chr_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                         uint16_t offset, uint8_t flags);

/* User API's */

ssize_t bt_get_attr_value_and_len(const struct bt_uuid *uuid, void *value, uint16_t *len);

/**
 * @brief Sets the read callback for a characteristic identified by its UUID.
 *
 * Can be set to NULL to disable the read callback for the characteristic.
 *
 * @param uuid UUID of characteristic to set the read callback for
 * @param read_cb Function pointer of the read callback to be set
 * @return The following status codes:
 *
 * - BT_ATT_ERR_SUCCESS: If at least one connection is subscribed and notification was sent.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE): If the characteristic with the given UUID is not found
 * or the characteristic entry is NULL.
 *
 */
ssize_t bt_gen_chr_set_read_cb(const struct bt_uuid *uuid, bt_gatt_handler_t read_cb);

/**
 * @brief Sets the write callback for a characteristic identified by its UUID.
 *
 * Can be set to NULL to disable the write callback for the characteristic.
 *
 * @param uuid UUID of characteristic to set the write callback for
 * @param write_cb Function pointer of the write callback to be set
 * @return The following status codes:
 *
 * - BT_ATT_ERR_SUCCESS: If at least one connection is subscribed and notification was sent.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE): If the characteristic with the given UUID is not found
 * or the characteristic entry is NULL.
 *
 */
ssize_t bt_gen_chr_set_write_cb(const struct bt_uuid *uuid, bt_gatt_handler_t write_cb);

/**
 * @brief Sets the notification transmission callback for a characteristic identified by its UUID.
 *
 * Can be set to NULL to disable the read notify for the characteristic.
 *
 * @param uuid UUID of characteristic to set the notify tx callback for
 * @param notify_cb Function pointer of the notify callback to be set
 * @return The following status codes:
 *
 * - BT_ATT_ERR_SUCCESS: If at least one connection is subscribed and notification was sent.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE): If the characteristic with the given UUID is not found
 * or the characteristic entry is NULL.
 *
 */
ssize_t bt_gen_chr_set_notify_tx_cb(const struct bt_uuid *uuid, bt_gatt_handler_t notify_cb);

/**
 *
 * @brief Notification handler for server characteristics, handles sending notifications from the device's
 * own characteristics
 *
 * Accepts partial data and sends notifications to all subscribed connections.
 *
 * @param uuid UUID of the characteristic to notify
 * @param data Data to be sent in the notification
 * @param len Length of the data to be sent
 * @return The following status codes:
 *
 * - BT_ATT_ERR_SUCCESS: If at least one connection is subscribed and notification was sent.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_CCC_IMPROPER_CONF): If no connection is subscribed to the
 * characteristic.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE): If the characteristic with the given UUID is not found
 * or the characteristic entry is NULL.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN): If the length of the data exceeds the length of
 * the characteristic entry.
 */
ssize_t bt_gen_chr_notify_tx(const struct bt_uuid *uuid, const void *data, const uint16_t len);

#endif /* GATT_DB_H */