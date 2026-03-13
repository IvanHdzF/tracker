/* * @file user_api.h
 * @brief User API for BLE stack.
 *
 * This file contains the user-facing API for the BLE stack, including GAP and GATT operations.
 * It provides functions to start advertising, scanning, and interacting with GATT characteristics.
 */

#pragma once

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <stdint.h>

#include "ble_stack_types.h"


#ifdef __cplusplus
extern "C" {
#endif

/*---------- L2CAP CoC API Error Codes -------------*/

typedef enum
{
  COC_ERR_OK = 0,
  COC_ERR_INVALID_PARAM = 1,
  COC_ERR_BUSY = 2,
  COC_ERR_NOT_SUPPORTED = 3,
  COC_ERR_ALLOC = 4
} coc_api_error_t;

/*----------- GAP user functions -------------*/

/**
 * @brief Start BLE advertising, initializing the advertisement data and scan response data and
 * start BLE module.
 *
 * This function must be called before any other BLE operations.
 *
 * @return Signed Integer (BLE Enable status code):
 *
 *        - 0 on success
 *
 *        - Negative error code on failure, see errrno.h for details.
 */
int ble_gap_adv_start(void);

/**
 * @brief Start BLE scanning, initializing the scan parameters and starting the scan.
 *
 * This function must be called after ble_adv_start.
 *
 * @return Signed Integer (BLE Scan status code):
 *
 *        - 0 on success
 *
 *        - Negative error code on failure, see errno.h for details.
 */
int ble_gap_scan_init(void);

/*----------- GATT user functions -------------*/

/**
 * @brief Get the value and length of a characteristic identified by its UUID.
 *
 * @param uuid uuid of the characteristic to read, check the util/uuids.h file for available UUIDs.
 * @param value buffer to store the characteristic value.
 * @param len length of the buffer to store the characteristic value, will be updated with the
 * actual length of the characteristic value.
 * @return 0 on success, negative error code otherwise, see att.h for details.
 */
int ble_gatt_get_char_value_and_len(const struct bt_uuid *uuid, void *value, uint16_t *len);


/**
 * @brief Register a callback for receiving generic RX notifications.
 *
 * This callback is used for receiving notifications on characteristics that do not have a specific
 * RX callback set. (INTAN data, etc.)
 *
 * @param cb
 * @return int
 */
int ble_gatt_set_gen_rx_notification_cb(gen_rx_notif_cb_func cb);

/**
 * @brief Set the read callback for a characteristic by UUID.
 *
 * @param uuid uuid of the characteristic to read, check the util/uuids.h file for available UUIDs.
 * @param cb   Callback function for read operations.
 * @return The following status codes:
 *
 * - BT_ATT_ERR_SUCCESS: If at least one connection is subscribed and notification was sent.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE): If the characteristic with the given UUID is not found
 * or the characteristic entry is NULL.
 *
 */
int ble_gatt_set_read_cb(const struct bt_uuid *uuid, bt_gatt_handler_t cb);

/**
 * @brief Set the write callback for a characteristic by UUID.
 *
 * @param uuid uuid of the characteristic to read, check the util/uuids.h file for available UUIDs.
 * @param cb   Callback function for write operations.
 *
 * - BT_ATT_ERR_SUCCESS: If at least one connection is subscribed and notification was sent.
 *
 * - BT_GATT_ERR(BT_ATT_ERR_INVALID_HANDLE): If the characteristic with the given UUID is not found
 * or the characteristic entry is NULL.
 *
 */
int ble_gatt_set_write_cb(const struct bt_uuid *uuid, bt_gatt_handler_t cb);


/**
 * @brief Send a notification to all subscribed connections for a characteristic.
 *
 * @param uuid uuid of the characteristic to read, check the util/uuids.h file for available UUIDs.
 * @param data Pointer to the data to notify.
 * @param len  Length of the data.
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
int ble_gatt_notify(const struct bt_uuid *uuid, const void *data, uint16_t len);

/*----------- Initialization of services -------------*/
/**
 * @brief Initialize BLE services.
 *
 * Initialize the NUS (Nordic UART Service) for BLE communication.
 *
 * This function sets up the necessary GATT characteristics and handles for the NUS service.
 *
 * @return 0 on success, negative error code otherwise.
 */
int ble_gatt_services_init(void);

/**
 * @brief Read a GATT characteristic by its 128-bit UUID.
 *
 * Initiates a GATT read operation on the characteristic identified by the given UUID.
 *
 * @param uuid_128 Pointer to the 128-bit UUID of the characteristic to read.
 * @return 0 on success, negative error code otherwise.
 */
int ble_gatt_read_chrc(const struct bt_uuid *uuid);

/**
 * @brief Write data to a GATT characteristic by its 128-bit UUID.
 *
 * Initiates a GATT write operation on the characteristic identified by the given UUID.
 *
 * @param uuid Pointer to the 128-bit UUID of the characteristic to write.
 * @param data Pointer to the data buffer to write.
 * @param len Length of the data to write.
 * @return 0 on success, negative error code otherwise.
 */
int ble_gatt_write_chrc(const struct bt_uuid *uuid, const void *data, const uint16_t len);

/**
 * @brief Write data to a GATT characteristic by its 128-bit UUID without waiting for a response.
 *
 * Initiates a GATT write without response operation on the characteristic identified by the given
 * UUID. This is typically used for high-throughput or low-latency data transfer where
 * acknowledgment is not required.
 *
 * @param uuid Pointer to the 128-bit UUID of the characteristic to write.
 * @param data Pointer to the data buffer to write.
 * @param len Length of the data to write.
 * @return 0 on success, negative error code otherwise.
 */
int ble_gatt_write_no_resp_chrc(const struct bt_uuid *uuid, const void *data, const uint16_t len);
/*----------- L2CAP CoC user functions -------------*/
/**
 * @brief Send data over the L2CAP CoC channel.
 * @param data Pointer to the data to send.
 * @param len  Length of the data to send.
 * @return coc_api_error_t status code indicating the result of the operation.
 *
 * - COC_ERR_OK: If the data was sent successfully.
 * - COC_ERR_INVALID_PARAM: If the data length exceeds the MTU.
 * - COC_ERR_NOT_SUPPORTED: If the L2CAP CoC channel is not connected or not initialized.
 */
coc_api_error_t ble_coc_send_data(uint8_t *data, size_t len);

/* RX Callback function definition */
typedef uint8_t (*coc_api_rx_callback)(const void *data, uint16_t len);

/**
 * @brief Register a callback for  L2CAP CoC
 * @param coc_api_rx_callback Pointer to the callback function to register.
 * @return coc_api_error_t status code indicating the result of the operation.
 *
 * - COC_ERR_OK: If the data was sent successfully.
 * - COC_ERR_INVALID_PARAM: NULL pointer passed as parameter.
 */
coc_api_error_t ble_coc_register_rx_callback(coc_api_rx_callback callback);

/**
 * @brief Initializes a CoC server for data reception
 * 
 * @return coc_api_error_t 
 */
coc_api_error_t ble_coc_init_server(void);

#ifdef __cplusplus
}
#endif
