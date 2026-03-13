#ifndef VARIABLES_H
#define VARIABLES_H

#define MAX_CHARACTERISTIC_LENGTH 512        /* According to BLE specs */
#define CALIBRATION_DATA_BUFFER_SIZE 512 - 4 /* 512 bytes - 4 bytes for address */

#define BLE_DISCOVERY_MODE_CID 0
#define BLE_DISCOVERY_MODE_GEN 1

#define BLE_DISCOVERY_MODE BLE_DISCOVERY_MODE_CID

#endif /* VARIABLES_H */