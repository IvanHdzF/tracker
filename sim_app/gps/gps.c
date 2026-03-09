#include "sim_app/gps/gps.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/cellular.h>
#include <zephyr/drivers/modem/simcom-sim7000.h>
#include <stdint.h>


LOG_MODULE_REGISTER(simcom_gps, LOG_LEVEL_INF);

int sim_gps_start(void)
{
    int ret;

    mdm_sim7000_stop_network();

    ret = mdm_sim7000_start_gnss();
    if (ret < 0) {
        LOG_ERR("Could not start GNSS!");
        return ret;
    }

    struct sim7000_gnss_data data = {0};

    for (size_t i = 0; i < 10; i++) {

        ret = mdm_sim7000_query_gnss(&data);

        if (ret == -EAGAIN) {
            LOG_WRN("GNSS fix not available yet");
        }

        k_msleep(1000);
    }

    ret = mdm_sim7000_stop_gnss();
    if (ret < 0) {
        LOG_ERR("Could not stop GNSS!");
        return ret;
    }

    mdm_sim7000_start_network();

    return 0;
}