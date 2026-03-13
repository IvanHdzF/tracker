#include "ble_stack/gap/adv.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "util/context.h"
#include "ble_stack/gap/conn.h"
#include <zephyr/kernel.h>

/* Device information */
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define BLE_TIMEOUT_MS			K_MSEC(5000)

LOG_MODULE_REGISTER(gap_adv, LOG_LEVEL_DBG);

K_SEM_DEFINE(ble_ready, 0, 1);

/* Advertise work queue */
static struct k_work advertise_work;

/* Advertising parameters */
#define BT_LE_ADV_CONN_PARAMS                                                                                 \
  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_1, BT_GAP_ADV_FAST_INT_MAX_1, NULL), ad, \
      ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd)

#define USER_ADV_DATA_IDX 2
static struct bt_data ad[] = {
    /* Bluetooth flags and device name */
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),

    /* User data */
    {0}};

/* Scan data */
static struct bt_data sd[] = {
    /* Service advertising */
    
    /* TODO: Advertise custom services */
};

static void init_adv_data(void)
{
  /* Nothing to do right now here, fill as needed */
}

/* Work queue helpers */
static void advertise(struct k_work *work)
{
  int err;

  err = bt_le_adv_start(BT_LE_ADV_CONN_PARAMS);
  if (err)
  {
    LOG_ERR("Failed to initialize ADV (err: %d)", err);
  }
  else
  {
    LOG_INF("ADV Initialized correctly");
  }
}

/* Advertisement helpers */
static void bt_ready(int err)
{
	if (err != 0) {
		LOG_ERR("Bluetooth failed to initialise: %d", err);
	} else {
		/* Load saved settings */
		LOG_INF("BLE is ready");
		if (IS_ENABLED(CONFIG_SETTINGS))
		{
			settings_load();
		}
		bt_set_name("MA_WCU");
		init_adv_data();
		adv_work_start();	
		k_sem_give(&ble_ready);
	}
}

/* Exposed functions */
int adv_start(void)
{
  int rc;

  /* Initialize advertising procedure */

	k_work_init(&advertise_work, advertise);
	rc = bt_enable(bt_ready);

  if (rc != 0)
  {
    LOG_ERR("Bluetooth enable failed: %d", rc);
  }

	rc = k_sem_take(&ble_ready, BLE_TIMEOUT_MS);
	if (rc != 0) {
		LOG_ERR("FATAL ERROR: Bluetooth enabling timeout'd");
	}
  rc = ble_bonding_init();

	return rc;
}

int adv_work_start(void)
{
  k_work_submit(&advertise_work);
  return 0;
}