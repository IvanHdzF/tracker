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


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define TEST_SERVER_PORT     80
#define TEST_SERVER_ENDPOINT "example.com"

/* CoAP configuration */
#define MESSAGE_TO_SEND "Hi from Lilygo T SIM7000G device"
#define APP_COAP_VERSION 1
#define APP_COAP_MAX_MSG_LEN 1280
#define COAP_RX_RESOURCE "validate"

/* -------------------------------------------------------------------------- */
/* Globals                                                                    */
/* -------------------------------------------------------------------------- */

/* CoAP state */
/* TODO: move to coap helper layer */
static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];
static uint16_t next_token;


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


/* -------------------------------------------------------------------------- */
/* CoAP Response Handling                                                     */
/* -------------------------------------------------------------------------- */

/* TODO: move to coap helper layer */
int client_handle_response(uint8_t *buf, int received)
{
    int err;
    struct coap_packet reply = {0};

    uint8_t token_len;
    uint16_t payload_len = 0;

    uint8_t token[8];
    uint8_t temp_buf[128];

    err = coap_packet_parse(&reply, buf, received, NULL, 0);
    if (err < 0) {
        LOG_ERR("Malformed response received: %d", err);
        return err;
    }

    token_len = coap_header_get_token(&reply, token);

    if ((token_len != sizeof(next_token)) ||
        (memcmp(&next_token, token, sizeof(next_token)) != 0)) {

        LOG_ERR("Invalid token received: 0x%02x%02x", token[1], token[0]);
        return 0;
    }

    const uint8_t *payload =
        coap_packet_get_payload(&reply, &payload_len);

    if (payload_len > 0) {
        snprintf(temp_buf,
                 MIN(payload_len + 1, sizeof(temp_buf)),
                 "%s",
                 payload);
    } else {
        strcpy(temp_buf, "EMPTY");
    }

    LOG_INF("CoAP response: Code 0x%x, Token 0x%02x%02x\nPayload: %s",
            coap_header_get_code(&reply),
            token[1],
            token[0],
            (char *)temp_buf);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Application Entry                                                          */
/* -------------------------------------------------------------------------- */

int main(void)
{
    int ret;
    int received;

    if(sim_modem_check_ready() != 0)
    {
        LOG_ERR("Modem device not ready");
        return -1;
    }

    LOG_INF("Starting SIM7000 Zephyr sample");


    sim_modem_print_info();

    // /* Early return for basic bringup testing */
    return 0;

    /* ------------------------------------------------------------------ */
    /* Socket test                                                        */
    /* ------------------------------------------------------------------ */

    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock < 0) {
        LOG_ERR("Socket create failed (%d)", errno);
        return -errno;
    }

    struct sockaddr_in server_addr = {0};

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5683);

    if (net_addr_pton(AF_INET,
                      "20.47.97.44",
                      &server_addr.sin_addr) != 0) {

        LOG_ERR("Error converting IPv4 address");
        return -1;
    }

    LOG_INF("Connecting to %s:%d", "20.47.97.44", 5683);

    ret = zsock_connect(sock,
                        (struct sockaddr *)&server_addr,
                        sizeof(server_addr));

    if (ret < 0) {

        LOG_ERR("Socket connect failed (%d)", errno);
        zsock_close(sock);

        return -errno;
    }

    LOG_INF("UDP connection successful");

    /* ------------------------------------------------------------------ */
    /* CoAP request                                                       */
    /* ------------------------------------------------------------------ */

    struct coap_packet request;

    next_token = sys_rand32_get();

    int err = coap_packet_init(
        &request,
        coap_buf,
        sizeof(coap_buf),
        APP_COAP_VERSION,
        COAP_TYPE_NON_CON,
        sizeof(next_token),
        (uint8_t *)&next_token,
        COAP_METHOD_GET,
        coap_next_id());

    if (err < 0) {
        LOG_ERR("Failed to create CoAP request (%d)", err);
        return err;
    }

    err = coap_packet_append_option(
        &request,
        COAP_OPTION_URI_PATH,
        (uint8_t *)COAP_RX_RESOURCE,
        strlen(COAP_RX_RESOURCE));

    if (err < 0) {
        LOG_ERR("Failed to encode CoAP option (%d)", err);
        return err;
    }

    err = zsock_send(sock, request.data, request.offset, 0);

    if (err < 0) {
        LOG_ERR("Failed to send CoAP request (%d)", errno);
        return -errno;
    }

    LOG_INF("CoAP GET request sent: Token 0x%04x", next_token);

    received = zsock_recv(sock, coap_buf, sizeof(coap_buf), 0);

    if (received < 0) {

        LOG_ERR("Socket error: %d", errno);

    } else if (received == 0) {

        LOG_INF("Empty datagram");
    }

    err = client_handle_response(coap_buf, received);

    if (err < 0) {
        LOG_ERR("Invalid response");
    }

    zsock_close(sock);

    LOG_INF("Sample complete");

    return 0;
}