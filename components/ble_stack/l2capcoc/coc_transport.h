
#include <zephyr/bluetooth/bluetooth.h>

#include "ble_stack/user_api.h"

#if IS_ENABLED(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
coc_api_error_t ble_coc_init(void);
coc_api_error_t ble_coc_connect(struct bt_conn *conn);
#else
static inline coc_api_error_t ble_coc_init(void)
{
  return COC_ERR_NOT_SUPPORTED;
}

static inline coc_api_error_t ble_coc_connect(struct bt_conn *conn)
{
  return COC_ERR_NOT_SUPPORTED;
}

#endif // CONFIG_BT_GATT_DB_AUTH_AND_ENCRYPTION