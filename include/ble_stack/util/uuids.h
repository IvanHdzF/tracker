#ifndef UUIDS_H
#define UUIDS_H

#include <zephyr/bluetooth/uuid.h>

/* Temporary UUID helper file intended for easier user interaction with the API */

/* Internal Service UUID's */
#define INTERNAL_UUID_DIS BT_UUID_DIS
#define INTERNAL_UUID_CTS BT_UUID_CTS


/* Internal Characteristics UUID's */
/* Device Information Service - Characteristics */
#define INTERNAL_UUID_DIS_CHAR_MODEL BT_UUID_DIS_MODEL_NUMBER
#define INTERNAL_UUID_DIS_CHAR_MANUFACTURER BT_UUID_DIS_MANUFACTURER_NAME
#define INTERNAL_UUID_DIS_CHAR_FW_VER BT_UUID_DIS_FIRMWARE_REVISION
#define INTERNAL_UUID_DIS_CHAR_SW_VER BT_UUID_DIS_SOFTWARE_REVISION
#define INTERNAL_UUID_DIS_CHAR_SN BT_UUID_DIS_SERIAL_NUMBER
#define INTERNAL_UUID_DIS_CHAR_SYS_ID BT_UUID_DIS_SYSTEM_ID

/* Current Time Service - Characteristics */
#define INTERNAL_UUID_CTS_CHAR_CURRENT_TIME BT_UUID_CTS_CURRENT_TIME

#endif /* UUIDS_H */