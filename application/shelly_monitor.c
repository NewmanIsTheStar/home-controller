#define _GNU_SOURCE
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
// #include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include <hardware/flash.h>

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>
#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/apps/http_client.h"
#include "lwip/pbuf.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "queue.h"

#include "stdarg.h"

// #include "weather.h"
#include "flash.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
// #include "powerwall.h"
#include "shelly.h"
#include "json_parser.h"
#include "pluto.h"
#include "ping_core.h"
#include "shell.h"

// defines
#define HTTP_PORT 80
#define MDNS_PORT 5353
#define MDNS_MULTICAST_IP "224.0.0.251"

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

// global variables
static QueueHandle_t device_queue = NULL;
// char mdns_text[512];

// prototypes

// #include <string.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include "pico/cyw43_arch.h"
// #include "lwip/sockets.h"
// #include "mbedtls/sha1.h"
// #include "mbedtls/base64.h"
// #include "FreeRTOS.h"
// #include "task.h"

#define BUFFER_SIZE 1024
#define SERVER_IP "192.168.33.163" // shelly device acts as server for websocket connection
#define SERVER_PORT 80

// Hardcoded RFC-compliant 16-byte base64 encoded client key for simplicity 
// Real applications could generate this dynamically using the RP2350 hardware TRNG 
#define CLIENT_WS_KEY "dGhlIHNhbXBsZSBub25jZQ=="

// Helper to send an unmasked client-to-server text frame 
// Per RFC 6455, client-to-server frames MUST be masked.

static void send_client_websocket_text(int socket_fd, const char *message)
{
    size_t payload_len = strlen(message);
    uint8_t *frame = pvPortMalloc(BUFFER_SIZE);
    if (!frame)
        return;
    size_t idx = 0;
    frame[idx++] = 0x81; // FIN bit set, Opcode 0x1 (Text)

    // Clients MUST set the mask bit (0x80) on the length byte
    if (payload_len <= 125)
    {
        frame[idx++] = (uint8_t)payload_len | 0x80;
    }
    else if (payload_len <= 65535)
    {
        frame[idx++] = 126 | 0x80;
        frame[idx++] = (uint8_t)(payload_len >> 8);
        frame[idx++] = (uint8_t)(payload_len & 0xFF);
    }
    else
    {
        vPortFree(frame);
        return; // Payload too large for this buffer size
    }

    // 4-byte masking key (using a simple static key for demonstration)
    uint8_t masking_key[4] = {0x11, 0x22, 0x33, 0x44};
    memcpy(&frame[idx], masking_key, 4);
    idx += 4;

    // Apply the XOR mask to the payload data
    for (size_t i = 0; i < payload_len; i++)
    {
        frame[idx + i] = (uint8_t)message[i] ^ masking_key[i % 4];
    }
    idx += payload_len;

    send(socket_fd, frame, idx, 0);
    vPortFree(frame);
}

// Helper to parse incoming unmasked text frames from the server 
static int parse_server_websocket_frame(int socket_fd, char *out_payload, size_t max_out_len, uint8_t *opcode)
{
    uint8_t header[2];
    int n = recv(socket_fd, header, 2, 0);
    if (n <= 0)
        return (n == 0) ? 0 : -1;
    *opcode = header[0] & 0x0F;
    int is_masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126)
    {
        uint16_t ext_len;
        if (recv(socket_fd, &ext_len, 2, 0) <= 0)
            return -1;
        payload_len = ntohs(ext_len);
    }

    // Server-to-client frames should NOT be masked per RFC 6455
    uint8_t masking_key[4] = {0};
    if (is_masked)
    {
        if (recv(socket_fd, masking_key, 4, 0) <= 0)
            return -1;
    }

    if (payload_len >= max_out_len)
        return -1;

    size_t total_received = 0;
    while (total_received < payload_len)
    {
        int r = recv(socket_fd, out_payload + total_received, payload_len - total_received, 0);
        if (r <= 0)
            return -1;
        total_received += r;
    }
    out_payload[payload_len] = '\0';

    if (is_masked)
    {
        for (size_t i = 0; i < payload_len; i++)
        {
            out_payload[i] ^= masking_key[i % 4];
        }
    }

    return (int)payload_len;
}

// FreeRTOS Client Task Loop 
void websocket_client_task(void *pvParameters)
{
    int socket_fd;
    struct sockaddr_in server_addr;
    char *buffer = pvPortMalloc(BUFFER_SIZE);
    if (!buffer)
        vTaskDelete(NULL);

    // 1. Setup standard blocking TCP Socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        vPortFree(buffer);
        vTaskDelete(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    printf("Connecting to WebSocket server at %s:%d...\n", SERVER_IP, SERVER_PORT);

    // Attempt connection (Ensure server task is running first!)
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Connection failed.\n");
        close(socket_fd);
        vPortFree(buffer);
        vTaskDelete(NULL);
    }

    // 2. Transmit the Handshake Upgrade Request
    int handshake_len = snprintf(buffer, BUFFER_SIZE,
                                 "GET / HTTP/1.1\r\n"
                                 "Host: %s:%d\r\n"
                                 "Upgrade: websocket\r\n"
                                 "Connection: Upgrade\r\n"
                                 "Sec-WebSocket-Key: %s\r\n"
                                 "Sec-WebSocket-Version: 13\r\n\r\n",
                                 SERVER_IP, SERVER_PORT, CLIENT_WS_KEY);

    send(socket_fd, buffer, handshake_len, 0);

    // 3. Await and read Handshake Upgrade Response
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_read > 0 && strstr(buffer, "101 Switching Protocols") != NULL)
    {
        printf("Handshake Successful! Connected to WebSocket Server.\n");

        // Send an initial message to the server
        send_client_websocket_text(socket_fd, "{\"id\": 1, \"src\": \"my_client\", \"method\": \"Shelly.GetStatus\"}");

        // 4. Client WebSocket Event Loop
        while (1)
        {
            uint8_t opcode = 0;
            int frame_len = parse_server_websocket_frame(socket_fd, buffer, BUFFER_SIZE, &opcode);

            if (frame_len <= 0)
            {
                printf("Server disconnected or connection error.\n");
                break;
            }

            if (opcode == 0x1)
            { // Text frame
                printf("[From Server]: %s\n", buffer);

                // Optional: Wait 2 seconds and echo back an automated heart beat message
                vTaskDelay(pdMS_TO_TICKS(2000));
                send_client_websocket_text(socket_fd, "Ping loop heartbeat");
            }
            else if (opcode == 0x8)
            {
                printf("Server sent close frame.\n");
                break;
            }
        }
    }
    else
    {
        printf("Handshake rejected or failed. Server response:\n%s\n", buffer);
    }

    // Clean up
    close(socket_fd);
    vPortFree(buffer);
    printf("Client task ended.\n");
    vTaskDelete(NULL);
}

/*
At the very beginning of the mDNS UDP payload (the first 12 bytes), there are 4 fixed counters:
Questions Count (2 bytes)
Answer RRs Count (2 bytes) — Shelly discovery details live here
Authority RRs Count (2 bytes)
Additional RRs Count (2 bytes) — IP addresses and TXT records live here

The primary parsing loop must parse exactly Answers + Authority + Additional records. Once your loop has run that many total times,
  you have fully extracted every name and payload inside that packet.
*/

/*
    • Enable IGMP: In your lwipopts.h configuration, you must define #define LWIP_IGMP 1. Without this, the lwIP stack will drop the inbound local mDNS multicast packets.
    • Core Pinning: Use xTaskCreateAtCore from the Pico SDK/FreeRTOS port. Keep the wireless network thread (cyw43_arch_poll) running tightly on Core 1, and spawn your
      validation workers and scanner loops on Core 0 to eliminate thread jitter.
    • TCP Non-Blocking Timeouts: Shelly devices on sleep mode or slow Wi-Fi links can hang threads. Set the lwIP Netconn receive timeout (netconn_set_rcvtimeo(conn, 2000))
      to a maximum of 2 seconds so a dead device doesn't lock up a worker task.
*/

/*
By default, Shelly Gen 1 devices use the standard CoAP multicast address 224.0.1.187 on UDP port 5683 for CoIoT multicast (mcast) communications

Use Inbound WebSockets (Best for Multi-Hub Local Control)While the device only supports one outbound connection, its local Inbound WebSocket endpoint
 (ws://<SHELLY_IP>/rpc) can handle multiple concurrent connections from different clients.How to use it: Instead of forcing the Shelly to dial out to the hubs,
  configure your smart home platforms (such as Home Assistant and Hubitat) to independently connect to the Shelly device directly. Both hubs can stay connected,
  poll, and receive push events over this inbound connection at the exact same time.

 Share Data via MQTTIf you need true standalone, multiple-client status delivery where the device handles pushing data out, MQTT is the industry standard.
 How to use it: Enable MQTT in the Shelly settings. Have the Shelly publish its state changes to a centralized MQTT Broker (like Mosquitto). Any number of
  distinct smart home hubs can subscribe to that broker simultaneously to get immediate updates without placing any extra load on the Shelly hardware.
*/