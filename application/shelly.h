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

#endif
