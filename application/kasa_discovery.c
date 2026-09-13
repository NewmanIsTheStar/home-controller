
/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include "string.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
// #include "generated/ws2812.pio.h"

// Prune this list of includes
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>
#include "lwip/api.h"
#include "lwip/sys.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "stdarg.h"

// #include "weather.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"
// #include "led_strip.h"
#include "udp.h"
// #include "message.h"
// #include "message_defs.h"
// #include "powerwall.h"
#include "shelly.h"
#include "discovery_task.h"





#define KASA_UDP_PORT 9999
//#define BUFFER_SIZE 512
#define BUFFER_SIZE 1024

// Encrypt / Decrypt in-place for UDP (No 4-byte header used in UDP)
void kasa_udp_cipher(uint8_t *buf, size_t len) {
    uint8_t key = 171; // Legacy Initialization Vector
    for (size_t i = 0; i < len; i++) {
        uint8_t next_byte = buf[i];
        buf[i] = next_byte ^ key;
        
        // For encryption, key becomes the ciphertext byte.
        // For decryption, key becomes the input ciphertext byte.
        // Since it's a symmetric rolling stream, this logic handles both.
        key = buf[i]; 
    }
}

// Encryption version optimized specifically for sending
void kasa_udp_encrypt(uint8_t *buf, size_t len) {
    uint8_t key = 171;
    for (size_t i = 0; i < len; i++) {
        uint8_t next_key = buf[i] ^ key;
        buf[i] = next_key;
        key = next_key;
    }
}

void kasa_discovery_task(void *pvParameters) {
    struct netconn *conn;
    struct netbuf *tx_netbuf, *rx_netbuf;
    err_t err;

    // Define the raw discovery payload
    const char *discovery_cmd = "{\"system\":{\"get_sysinfo\":{}}}";
    size_t cmd_len = strlen(discovery_cmd);

    uint8_t tx_payload[128];

    for(;;)
    {
        memcpy(tx_payload, discovery_cmd, cmd_len);
        kasa_udp_encrypt(tx_payload, cmd_len);

        // Create UDP Netconn
        conn = netconn_new(NETCONN_UDP);
        if (conn == NULL) {
            vTaskDelete(NULL);
        }

        // Bind locally to allow reception of return packets on any interface
        err = netconn_bind(conn, IP_ADDR_ANY, 0);
        if (err != ERR_OK) {
            netconn_delete(conn);
            vTaskDelete(NULL);
        }

        // Allocate and configure transmitting network buffer
        tx_netbuf = netbuf_new();
        netbuf_ref(tx_netbuf, tx_payload, cmd_len);

        // Direct the packet to the global broadcast address on port 9999
        ip_addr_t broadcast_ip;
        //IP4_ADDR(&broadcast_ip, 255, 255, 255, 255);
        IP4_ADDR(&broadcast_ip, 192, 168, 33, 255);
        
        // Broadcast the discovery request
        err = netconn_sendto(conn, tx_netbuf, &broadcast_ip, KASA_UDP_PORT);
        netbuf_delete(tx_netbuf); // Done with sending buffer

        if (err == ERR_OK) {
            // Set a timeout for the incoming responses so the thread doesn't hang indefinitely
            netconn_set_recvtimeout(conn, 2000); // 2-second listening window

            printf("Scanning for Kasa Smart Plugs...\n");

            // Listen loop to capture all responsive Kasa devices on the network
            while (netconn_recv(conn, &rx_netbuf) == ERR_OK) {
                ip_addr_t *responder_ip = netbuf_fromaddr(rx_netbuf);
                uint16_t data_len;
                uint8_t *raw_data;

                netbuf_data(rx_netbuf, (void**)&raw_data, &data_len);

                if (data_len < BUFFER_SIZE) {
                    uint8_t rx_string[BUFFER_SIZE];
                    memcpy(rx_string, raw_data, data_len);
                    rx_string[data_len] = '\0'; // Null terminator for processing

                    // printf("before decrypt\n");
                    // hex_dump(rx_string, data_len);

                    // Decrypt payload in-place
                    kasa_udp_cipher(rx_string, data_len);

                    // printf("after decrypt\n");
                    // hex_dump(rx_string, data_len);

                    char ip_str[16];
                    ipaddr_ntoa_r(responder_ip, ip_str, sizeof(ip_str));

                    printf("Found Device at IP: %s\n", ip_str);
                    // printf("Response JSON: %s\n\n", rx_string);
                    
                    // Tip: Look for substrings like "\"model\":\"HS100\"" or "\"alias\":\"Living Room\"" 
                    // in 'rx_string' to match specific plugs dynamically.
                }
                netbuf_delete(rx_netbuf);
            }
        }

        // Clean up netconn
        netconn_close(conn);
        netconn_delete(conn);
        
        // tell watchdog task that we are still alive
        watchdog_pulse((int *)pvParameters); 

        printf("Kasa Discovery scan complete.\n");
        SLEEP_MS(60000);

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)pvParameters);          
    }

    vTaskDelete(NULL);
}
