#ifndef GATT_CLIENT_H
#define GATT_CLIENT_H

#include "ble_stack_types.h"

/**
 * @brief Generic characteristic RX notification callback
 *
 * @param data Pointer to the received data
 * @param len length of the received data
 */
void bt_gen_chr_rx_notification_cb(bt_gen_client_char_req_t* client_req);

/**
 * @brief Sets the Generic RX callback for the Generic characteristic.
 *
 * @param gen_rx_cb Function pointer to the Generic RX callback to be set. @ref gen_rx_notif_cb_func
 * @return The following status codes:
 *
 * - BT_ATT_ERR_SUCCESS: If the Generic RX callback was successfully set.
 */
ssize_t bt_gen_chr_set_gen_rx_cb(gen_rx_notif_cb_func gen_rx_cb);

/**
 * @brief Performs an operation over a gatt attribute (characteristic)
 *
 * @param uuid_128 UUID in 128 bit form of characteristic to perform an operation to
 * @param op_type Type of operation, @ref op_type_t
 * @param data Data for operation (Optional, only needed for writes)
 * @param len Length of data
 * @return int
 */
int bt_client_gen_chrc_do_op(const struct bt_uuid_128* uuid_128, uint8_t op_type, const void* data, const uint16_t len);

#endif /* GATT_CLIENT_H */