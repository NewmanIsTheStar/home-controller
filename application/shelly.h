#ifndef SHELLY_H
#define SHELLY_H

// #include "powerwall.h"
typedef enum
{
    HTTP_GET,
    HTTP_POST
} HTTP_REQUEST_TYPE_T;

int shelly_discover_devices(void);
int shelly_http_request(HTTP_REQUEST_TYPE_T type, char *url, char *host, char *content);
int shelly_cache_set_value(char *device_ip, char *device_parameter, char *parameter_value);
int shelly_cache_get_value(uint8_t *device_ip, char *parameter_name, char *value, size_t value_len);
int shelly_cache_insert_parameter(uint8_t device_index, char *device_parameter, uint8_t *parameter_index);
int shelly_cache_insert_device(uint8_t *device_ip, uint8_t *device_index);
int shelly_cache_clear(void);
int shelly_cache_dump(void);
int shelly_cache_device_dump(char *ipv4_string);
int strip_quotes(char *dst, char *src);
void http_validator_task(void *pvParameters);
void init_mdns_scanner();
void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
void websocket_client_task(void *pvParameters);

#endif
