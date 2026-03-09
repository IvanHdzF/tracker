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



int main(void)
{
    int err;

    if(sim_modem_check_ready() != 0)
    {
        LOG_ERR("Modem device not ready");
        return -1;
    }

    LOG_INF("Starting SIM7000 Zephyr sample");


    sim_modem_print_info();

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
        return err;
    }

    err = coap_app_send_get_request(sock, COAP_RX_RESOURCE);
    if (err < 0) {
        LOG_ERR("Failed to send CoAP request");
        return err;
    }

    err = coap_app_receive_message(sock, buffer, sizeof(buffer));
    if (err < 0) {
        LOG_ERR("Failed to receive CoAP response");
        return err;
    }


    /* Cleanup */
    err = coap_app_disconnect_from_server(sock);
    if (err < 0) {
        LOG_ERR("Failed to disconnect from CoAP server");
        return err;
    }

    LOG_INF("Program completed successfully");

    return 0;
}