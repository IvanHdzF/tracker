#include "coap_app/coap.h"

#include <zephyr/logging/log.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/socket_offload.h>


#include <zephyr/net/coap.h>
#include <zephyr/random/random.h>

#include <string.h>

/* CoAP configuration */
#define APP_COAP_VERSION 1
#define APP_COAP_MAX_MSG_LEN 1280


LOG_MODULE_REGISTER(coap_app, LOG_LEVEL_INF);

static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];
static uint16_t next_token;


int client_handle_response(uint8_t *buf, int received)
{
    if (buf == NULL || received <= 0) {
        LOG_ERR("Invalid buffer or no data received");
        return -EINVAL;
    }

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

int coap_app_connect_to_server(const char *ip_addr, uint16_t port)
{
    if (ip_addr == NULL) {
        LOG_ERR("Invalid IP address");
        return -EINVAL;
    }

    int err = 0;
    int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock < 0) {
        LOG_ERR("Socket create failed (%d)", errno);
        return -errno;
    }

    struct sockaddr_in server_addr = {0};

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (net_addr_pton(AF_INET,
        ip_addr, &server_addr.sin_addr) != 0) {

        LOG_ERR("Error converting IPv4 address");
        return -1;
    }

    LOG_INF("Connecting to %s:%d", ip_addr, port);

    err = zsock_connect(sock,
                        (struct sockaddr *)&server_addr,
                        sizeof(server_addr));

    if (err < 0) {

        LOG_ERR("Socket connect failed (%d)", errno);
        zsock_close(sock);

        return -errno;
    }
    LOG_INF("UDP connection successful");

    return sock;
}

int coap_app_send_get_request(int sock, const char *uri_path)
{
    int err;
    struct coap_packet request;

    next_token = sys_rand32_get();

    err = coap_packet_init(
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
        (uint8_t *)uri_path,
        strlen(uri_path));

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
}

int coap_app_send_put_request(int sock, const char *uri_path, const char *payload, size_t payload_len)
{
    int err;
    struct coap_packet request;

    next_token = sys_rand32_get();

    err = coap_packet_init(
        &request,
        coap_buf,
        sizeof(coap_buf),
        APP_COAP_VERSION,
        COAP_TYPE_NON_CON,
        sizeof(next_token),
        (uint8_t *)&next_token,
        COAP_METHOD_PUT,
        coap_next_id());
    

    if (err < 0) {
        LOG_ERR("Failed to create CoAP request (%d)", err);
        return err;
    }

    err = coap_packet_append_option(
        &request,
        COAP_OPTION_URI_PATH,
        (uint8_t *)uri_path,
        strlen(uri_path));

    if (err < 0) {
        LOG_ERR("Failed to encode CoAP option (%d)", err);
        return err;
    }

    const uint8_t text_plain = COAP_CONTENT_FORMAT_TEXT_PLAIN;
	err = coap_packet_append_option(&request, COAP_OPTION_CONTENT_FORMAT,
					&text_plain,
					sizeof(text_plain));
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option, %d\n", err);
		return err;
	}

    err = coap_packet_append_payload_marker(&request);
	if (err < 0) {
		LOG_ERR("Failed to append payload marker, %d\n", err);
		return err;
	}

    err = coap_packet_append_payload(&request, (uint8_t *)payload, payload_len);
	if (err < 0) {
		LOG_ERR("Failed to append payload, %d\n", err);
		return err;
	}

    err = zsock_send(sock, request.data, request.offset, 0);

    if (err < 0) {
        LOG_ERR("Failed to send CoAP request (%d)", errno);
        return -errno;
    }

    LOG_INF("CoAP PUT request sent: Token 0x%04x", next_token);
    return 0;
}

int coap_app_receive_message(int sock, char *buffer, size_t buffer_len)
{
    int received = zsock_recv(sock, coap_buf, sizeof(coap_buf), 0);

    if (received < 0) {
        LOG_ERR("Socket error: %d", errno);
        return -errno;
    }

    return client_handle_response(coap_buf, received);
}

int coap_app_disconnect_from_server(int sock)
{
    if (sock < 0) {
        LOG_ERR("Invalid socket");
        return -EINVAL;
    }
    if (zsock_close(sock) != 0) {
        LOG_ERR("Failed to close socket (%d)", errno);
        return -errno;
    }
    LOG_INF("Disconnected from CoAP server");
    return 0;
}
