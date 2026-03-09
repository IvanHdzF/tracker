#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>


#include <zephyr/net/socket.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/socket_offload.h>

#include <zephyr/net/coap.h>
#include <zephyr/random/random.h>

#include "sim_app/modem/sim_modem.h"

#include <zephyr/drivers/cellular.h>
#include <zephyr/drivers/modem/simcom-sim7000.h>


static const struct device *modem = DEVICE_DT_GET(DT_ALIAS(modem));

LOG_MODULE_REGISTER(simcom_modem, LOG_LEVEL_INF);

int sim_modem_check_ready(void)
{
    if (!device_is_ready(modem)) {
        return -ENODEV;
    }
    return 0;
}

void sim_modem_print_info(void)
{
    /* ------------------------------------------------------------------ */
    /* Basic modem info                                                   */
    /* ------------------------------------------------------------------ */

    LOG_INF("Manufacturer: %s", mdm_sim7000_get_manufacturer());
    LOG_INF("Revision: %s", mdm_sim7000_get_revision());
    LOG_INF("IMEI: %s", mdm_sim7000_get_imei());
}