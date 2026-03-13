#include "sim_app/gps/gps.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/cellular.h>
#include <zephyr/drivers/modem/simcom-sim7000.h>
#include <stdint.h>
#include <time.h>

static postproc_fn_t postproc = NULL;

LOG_MODULE_REGISTER(simcom_gps, LOG_LEVEL_INF);

/* Helpers */

/*
 * Returns the difference in hours between two struct tm timestamps.
 * Positive result means local_time is later than inject_time.
 */
static int diff_hours_between_tm(const struct tm *local_time, const struct tm *inject_time)
{
    struct tm a = *local_time;   /* copy because mktime may modify fields */
    struct tm b = *inject_time;

    time_t ta = mktime(&a);
    time_t tb = mktime(&b);

    if (ta == (time_t)-1 || tb == (time_t)-1) {
        return 0; /* fallback if conversion fails */
    }

    double diff_sec = difftime(ta, tb);
    return (int)(diff_sec / 3600);
}


static void check_xtra_age(const struct tm *local_time, const struct tm *inject)
{
    int diff_h = diff_hours_between_tm(local_time, inject);

    LOG_INF("XTRA age: %d hours", diff_h);

    if (diff_h < 0)
    {
        LOG_INF("XTRA inject time is in the future. Update the modem clock before using XTRA");
        // TODO: Implement AT+CLTS=1 command on driver to let modem update its clock from network, and remove this log and check.

    }

    if (diff_h > 72) {
        LOG_INF("XTRA older than 72h, refreshing clock before using XTRA");
        // Update clock just in case, even if the inject time is not in the future. 
        // This is to avoid cases where the local time is wrong and the diff is small but still the xtra is old.

        
        // TODO: Implement AT+CLTS=1 command on driver to let modem update its clock from network, and remove this log and check.
        //return mdm_sim7000_download_xtra(1, "xtra3grc_72h.bin");
    }
}

int check_and_refresh_xtra(void)
{
    int16_t duration_h;
    struct tm inject, local_time;
    int ret;

    ret = mdm_sim7000_get_local_time(&local_time);
    if (ret) {
        LOG_ERR("Could not get local time!");
        return ret;
    }

    LOG_INF("Local time: %04d-%02d-%02d %02d:%02d:%02d",
        local_time.tm_year,
        local_time.tm_mon + 1,
        local_time.tm_mday,
        local_time.tm_hour,
        local_time.tm_min,
        local_time.tm_sec);

    ret = mdm_sim7000_query_xtra_validity(&duration_h, &inject);
    if (ret) {
        /* safest fallback: download */
        LOG_INF("Could not query xtra validity. Downloading new file");        
        //return mdm_sim7000_download_xtra(1, "xtra3grc_72h.bin");
    }
    

    /* Compare manually local time and inject time */
    check_xtra_age(&local_time, &inject);
    return 0;
}

static void wait_until_fix(void)
{
    int err = 0;
    struct sim7000_gnss_data data = {0};

    while (data.fix_status == false) {

        err = mdm_sim7000_query_gnss(&data);

        if (err == -EAGAIN) {
            LOG_WRN("GNSS fix not available yet");
        }

        if (postproc != NULL) {
            postproc(&data, sizeof(data));
        } else
        {
            LOG_WRN("Postprocess not set!");
        }

        k_msleep(1000);
    }
}

/* API */

int sim_gps_start(void)
{
    int ret;

    mdm_sim7000_stop_network();

    ret = mdm_sim7000_start_gnss();
    if (ret < 0) {
        LOG_ERR("Could not start GNSS!");
        return ret;
    }

    wait_until_fix();

    return 0;
}

int sim_gps_start_xtra(void)
{
    int ret;

    mdm_sim7000_start_network();

    ret = check_and_refresh_xtra();
    if (ret < 0) {
        LOG_ERR("Could not check and refresh xtra!");
        return ret;
    }

    mdm_sim7000_stop_network();

    ret = mdm_sim7000_start_gnss_xtra();
    if (ret < 0) {
        LOG_ERR("Could not start GNSS with XTRA!");
        return ret;
    }

    wait_until_fix();

    LOG_INF("GNSS fix acquired with XTRA!");
    return 0;
}

int sim_gps_get_data(struct sim7000_gnss_data *data)
{
    int ret = mdm_sim7000_query_gnss(data);
    if (ret < 0) {
        LOG_ERR("Failed to query GNSS data");
    }

    if (postproc != NULL) {
        postproc(data, sizeof(*data));
    } else
    {
        LOG_WRN("Postprocess not set!");
    }

    return ret;
}

int sim_gps_stop(void)
{
    return mdm_sim7000_stop_gnss();
}


void sim_gps_postproc_set(postproc_fn_t fn)
{
    postproc = fn;
}