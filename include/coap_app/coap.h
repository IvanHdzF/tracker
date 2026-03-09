#ifndef COAP_APP_H
#define COAP_APP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int coap_app_connect_to_server(const char *ip_addr, uint16_t port);
int coap_app_send_get_request(int sock, const char *uri_path);
int coap_app_send_put_request(int sock, const char *uri_path, const char *payload, size_t payload_len);
int coap_app_receive_message(int sock, char *buffer, size_t buffer_len);
int coap_app_disconnect_from_server(int sock);

#ifdef __cplusplus
}
#endif

#endif /* COAP_APP_H */