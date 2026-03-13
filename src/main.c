#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>


#include <zephyr/net/socket.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/socket_offload.h>

#include <zephyr/net/coap.h>
#include <zephyr/random/random.h>

#include <zephyr/drivers/cellular.h>
#include <zephyr/drivers/modem/simcom-sim7000.h>

#include "sim_app/gps/gps.h"
#include "sim_app/modem/sim_modem.h"
#include "coap_app/coap.h"
#include "ble_stack/user_api.h"

// TODO: Move this header to app layer instead
#include "ble_stack/gatt_db/service_cts.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define TEST_SERVER_PORT     5683
#define TEST_SERVER_ENDPOINT "20.47.97.44"

#define COAP_RX_RESOURCE "validate"
#define COAP_TX_RESOURCE "validate"


#define MESSAGE_TO_SEND "Hi from Lilygo T SIM7000G device"


/* -------------------------------------------------------------------------- */
/* DNS Helper                                                                 */
/* -------------------------------------------------------------------------- */

/* TODO: move to connectivity helper module */
#if defined(CIPS_MODEM_SUBSYS_IMPLEMENTED)
static int resolve_broker_addr(struct sockaddr_in *broker)
{
    struct zsock_addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct zsock_addrinfo *res;
    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%d", TEST_SERVER_PORT);

    int ret = zsock_getaddrinfo(TEST_SERVER_ENDPOINT,
                                port_str,
                                &hints,
                                &res);

    if (ret) {
        LOG_ERR("DNS failed (%d)", ret);
        return ret;
    }

    memcpy(broker, res->ai_addr, sizeof(struct sockaddr_in));

    zsock_freeaddrinfo(res);

    return 0;
}
#endif

#define LOC_SPEED_FLAG_POSITION_STATUS_SHIFT 7
#define LOC_SPEED_FLAG_POSITION_OK           (0x01 << LOC_SPEED_FLAG_POSITION_STATUS_SHIFT)

/* Helpers */

// TODO: Move got_fix to dedicated context later
static bool got_fix = false;
static int64_t gnss_start_time = 0;
int gps_query_post_process(const void* data, size_t len)
{
    LOG_INF("Post processing GPS data with custom function, data length: %d", len);

    if (!data) {
        LOG_ERR("Invalid GPS data");
        return -EINVAL;
    }

    if (len != sizeof(struct sim7000_gnss_data)) {
        LOG_ERR("Invalid GPS data size, got %u, expected %u", len, sizeof(struct sim7000_gnss_data));
        return -EINVAL;
    }

    const struct sim7000_gnss_data *gnss = data;

    loc_and_speed_data_t loc = {0};
    pos_qual_t pos_qual_data = {0};

    /* Only populate fields if we have a valid fix */
    if (gnss->fix_status) {

        /* Fill location and speed data first */
        /* Position status = 01 (Position OK) */
        loc.flags = LOC_AND_SPEED_DATA_FLAGS_MASK;
        loc.flags |= LOC_SPEED_FLAG_POSITION_OK;

        /* Latitude / Longitude (already 10^-7 degrees) */
        loc.location_latitude  = gnss->lat;
        loc.location_longitude = gnss->lon;

        /* Altitude: convert mm -> 24-bit signed */
        int32_t alt_mm = gnss->alt;

        loc.elevation[0] = (alt_mm >> 0) & 0xFF;
        loc.elevation[1] = (alt_mm >> 8) & 0xFF;
        loc.elevation[2] = (alt_mm >> 16) & 0xFF;

        /* Heading: convert 10^-2 degree -> 1e-2 degree format already correct */
        loc.heading = gnss->cog;

        /* Speed: km/h * 10 -> convert to m/s * 100 */
        /* 1 km/h = 0.27778 m/s */
        uint32_t speed_ms_100 = (gnss->kmh * 1000) / 36;

        loc.instant_speed = (uint16_t)speed_ms_100;

        /* Fill position quality data */
        pos_qual_data.flags = POSITION_QUALITY_FLAGS_MASK;
        /* Same position as location and speed */
        pos_qual_data.flags |= LOC_SPEED_FLAG_POSITION_OK;

        pos_qual_data.hdop = gnss->hdop / 10;
        // Pos_qual does not have a PDOP field
        pos_qual_data.vdop = gnss->vdop / 10;

        pos_qual_data.beacon_num_sol  = gnss->sat_used;
        pos_qual_data.beacon_num_view = gnss->sat_in_view;

        pos_qual_data.ephe = gnss->hpa;
        pos_qual_data.evpe = gnss->vpa;
        
        /* Update if it's first fix */
        if (!got_fix)
        {
            int64_t now = k_uptime_get();
            pos_qual_data.time_to_first_fix = (uint16_t)((now - gnss_start_time) / 1000);
        }
        
        got_fix = true;
    }


    ble_gatt_notify(BT_UUID_GATT_LOC_SPD, &loc, sizeof(loc));
    ble_gatt_notify(BT_UUID_GATT_PQ, &pos_qual_data, sizeof(pos_qual_data));
    return 0;
}

#define GPS_POLLING_MS  1000
static void gnss_poll_loop(void)
{
    int err = 0;
    LOG_INF("Starting GPS polling loop!");
    while (1)
    {
        struct sim7000_gnss_data data;
        err = sim_gps_get_data(&data);
        if (err < 0) {
            LOG_ERR("Failed to get GPS data");
        }
        LOG_INF("GPS Data: Latitude: %d, Longitude: %d, Altitude: %d",
                data.lat, data.lon, data.alt);
        k_msleep(GPS_POLLING_MS);
    }
    
}

/* Samples */
static int coap_sample(void)
{
    mdm_sim7000_start_network();

    int err = 0;
    int sock = coap_app_connect_to_server(TEST_SERVER_ENDPOINT, TEST_SERVER_PORT);
    if (sock < 0) {
        LOG_ERR("Failed to connect to CoAP server");
        return sock;
    }

    /* Read resource */

    err = coap_app_send_get_request(sock, COAP_RX_RESOURCE);
    if (err < 0) {
        LOG_ERR("Failed to send CoAP request");
        return err;
    }

    char buffer[128];
    err = coap_app_receive_message(sock, buffer, sizeof(buffer));
    if (err < 0) {
        LOG_ERR("Failed to receive CoAP response");
        return err;
    }

    /* Send request */
    coap_app_send_put_request(sock, COAP_RX_RESOURCE, MESSAGE_TO_SEND, strlen(MESSAGE_TO_SEND));

    err = coap_app_receive_message(sock, buffer, sizeof(buffer));
    if (err < 0) {
        LOG_ERR("Failed to receive CoAP response");
        goto out;
    }

    err = coap_app_send_get_request(sock, COAP_RX_RESOURCE);
    if (err < 0) {
        LOG_ERR("Failed to send CoAP request");
        goto out;
    }

    err = coap_app_receive_message(sock, buffer, sizeof(buffer));
    if (err < 0) {
        LOG_ERR("Failed to receive CoAP response");
        goto out;
    }

out:
    /* Cleanup */
    err = coap_app_disconnect_from_server(sock);
    if (err < 0) {
        LOG_ERR("Failed to disconnect from CoAP server");
        return err;
    }

    return err;
}

static int gps_sample(void)
{
    sim_gps_postproc_set(gps_query_post_process);
    gnss_start_time = k_uptime_get();
    int err = sim_gps_start();
    if (err < 0) {
        LOG_ERR("Failed to initialize GPS");
        return err;
    } 

    gnss_poll_loop();
    return 0;
}

static int gps_sample_xtra(void)
{
    gnss_start_time = k_uptime_get();
    sim_gps_postproc_set(gps_query_post_process);

    int err = sim_gps_start_xtra();
    if (err < 0) {
        LOG_ERR("Failed to initialize GPS with XTRA");
        return err;
    }

    struct sim7000_gnss_data data;
    err = sim_gps_get_data(&data);
    if (err < 0) {
        LOG_ERR("Failed to get GPS data");
        return err;
    }

    LOG_INF("GPS Data: Latitude: %d, Longitude: %d, Altitude: %d",
            data.lat, data.lon, data.alt);

    return 0;
}

uint8_t pos_qual_read_cb(const void *data, uint16_t len){
    if (len > sizeof(pos_qual_t)) {
        LOG_WRN("Data length %u exceeds pos_qual_t size %u, data will be truncated", len, sizeof(pos_qual_t));
        len = sizeof(pos_qual_t);
    }
    pos_qual_t pos_qual_data = {0};
    memcpy(&pos_qual_data, data, len);


    // TODO: Temporal hack for updating internal value, remove when refactoring GPS data out of the ble wrapper.
    ble_gatt_notify(BT_UUID_GATT_PQ, &pos_qual_data, sizeof(pos_qual_data));
    LOG_INF("Position Quality Data read callback invoked with data:\nflags: %u\nbeacon_num_sol: %u\nbeacon_num_view: %u\ntime_to_first_fix: %u\nephe: %u\nevpe: %u\nhdop: %u\nvdop: %u",
                pos_qual_data.flags, pos_qual_data.beacon_num_sol, pos_qual_data.beacon_num_view, pos_qual_data.time_to_first_fix,
                pos_qual_data.ephe, pos_qual_data.evpe, pos_qual_data.hdop, pos_qual_data.vdop);

    return 0;
}


int ble_init(void)
{
    int err;
    /* Initialize BLE and advertise*/
    err = ble_gap_adv_start();
    if (err)
    {
        LOG_ERR("Failed to initialize BT (err: %d)", err);
        return -1;
    }

    ble_gatt_set_read_cb(BT_UUID_GATT_PQ, pos_qual_read_cb);
    return 0;
}


int main(void)
{
    int err = 0;

    if(sim_modem_check_ready() != 0)
    {
        LOG_ERR("Modem device not ready");
        return -1;
    }

    LOG_INF("Starting SIM7000 Zephyr sample");



    sim_modem_print_info();

    ble_init();

    gps_sample();

    // coap_sample();

    LOG_INF("Program completed successfully");

    return err;
}