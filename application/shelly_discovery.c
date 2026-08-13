#define _GNU_SOURCE
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
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
//char mdns_text[512];

// prototypes
//int send_http_get(const ip_addr_t *target_addr, const char *path, char *response, size_t response_sz);
int send_http_get(const ip_addr_t *target_addr, const char *path, char *response, size_t response_sz, int *http_status);
u16_t parse_mdns_name_client(struct pbuf *p, u16_t offset, char *dest, u16_t dest_len);
int name_containing_string(struct pbuf *p, char *string_to_match);
void extract_all_names(struct pbuf *p);


// Callback triggered whenever an mDNS packet is received
void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        //u16_t offset = 0;
        // Parse the DNS payload looking for strings containing "shelly"
        // Extract the source IP address 'addr'
        char *payload = (char *)p->payload;
        
        //hex_dump(payload, p->len);

        //offset = parse_mdns_name_client(p, offset, mdns_text, sizeof(mdns_text));

        //if (strstr(payload, "shelly") != NULL) 
        //if (strstr(mdns_text, "shelly") != NULL) 
        if (name_containing_string(p, "shelly"))
        {
            // Push discovered IP to a FreeRTOS queue for the HTTP validation worker thread
            xQueueSendFromISR(device_queue, &addr->addr, NULL);
        }
        else
        {
           hex_dump(payload, p->len);
        }
        pbuf_free(p); // Always free the lwIP buffer
    }
}

void init_mdns_scanner() {
    struct udp_pcb *pcb = udp_new();
    ip_addr_t multicast_ip;
    ip4addr_aton(MDNS_MULTICAST_IP, &multicast_ip);
    err_t error = 0;
    
    // Join IGMP Multicast Group so the CYW43 chip passes the packets
    error = igmp_joingroup(IP_ADDR_ANY, &multicast_ip);
    printf("igmp_joingroup::%d\n", error);

    error = udp_bind(pcb, IP_ADDR_ANY, MDNS_PORT);
    printf("udp_bind::%d\n", error);

    udp_recv(pcb, mdns_recv_callback, NULL);
    printf("udp_recv:: no return value\n");
}




void http_validator_task(void *pvParameters) {
    ip_addr_t target_ip;
    int http_status = 0;
    int len;
    char response[512];

    device_queue = xQueueCreate(32, sizeof(ip_addr_t));

    while(1) {
        // Block until mDNS scanner or subnet sweeper finds a potential IP
        if (xQueueReceive(device_queue, &target_ip, portMAX_DELAY) == pdTRUE) {
            
            printf("VALIDATOR got message\n");

            // 1. Try Gen1 Check

            len = send_http_get(&target_ip, "/shelly", response, sizeof(response), &http_status);
            
            // if (len > 0)
            // {
            //     hex_dump(response, len);
            // }

            if (http_status == 200 && strstr(response, "\"type\":")) {
                printf("*********************************************************Found Gen1 Shelly Device at %s\n", ipaddr_ntoa(&target_ip));
                // Handle parsing firmware updates or URL mapping
            } 
            // 2. Try Gen2 Check if Gen1 fails or 404s
            else {
                len = send_http_get(&target_ip, "/rpc/Shelly.GetDeviceInfo", response, sizeof(response), &http_status);

                // if (len > 0)
                // {
                //     hex_dump(response, len);
                // }

                if (http_status == 200 && strstr(response, "\"gen\":2")) {
                    printf("*********************************************************Found Gen2 Shelly Device at %s\n", ipaddr_ntoa(&target_ip));
                }
                else
                {
                    printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX TOTAL FAILURE TO RECOGNIZE DEVICE\n");
                    if (len > 0)
                    {
                        hex_dump(response, len);
                    }
                }
            }
        }
    }
}


// /**
//  * @brief Performs an HTTP GET request to the specified target IP and path.
//  * 
//  * @param target_addr Pointer to the native lwIP IP address structure.
//  * @param path        The URI path (e.g., "/shelly").
//  * @param response    Pointer to the buffer where the response will be stored.
//  * @param response_sz The maximum size of the response buffer.
//  * @return int        The number of bytes received, or a negative error code on failure.
//  */
// int send_http_get(const ip_addr_t *target_addr, const char *path, char *response, size_t response_sz) {
//     if (target_addr == NULL || response == NULL || response_sz == 0) {
//         return -1;
//     }

//     struct sockaddr_in remote_addr;
//     memset(&remote_addr, 0, sizeof(remote_addr));
//     remote_addr.sin_family = AF_INET;
//     remote_addr.sin_port = lwip_htons(HTTP_PORT);

//     // Map lwIP's native ip_addr_t directly into the BSD socket sockaddr structure
//     #if LWIP_IPV4
//     remote_addr.sin_addr.s_addr = ip4_addr_get_u32(ip_2_ip4(target_addr));  //try ->addr
//     #else
//     remote_addr.sin_addr.s_addr = target_addr->addr;
//     #endif

//     // Allocate a stream (TCP) socket
//     int sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0) {
//         printf("Error: Failed to create socket\n");
//         return -3;
//     }

//     // Convert IP address back to string for display and the HTTP Host header
//     char ip_str[16];
//     ipaddr_ntoa_r(target_addr, ip_str, sizeof(ip_str));

//     printf("Connecting to Shelly device at %s:%d...\n", ip_str, HTTP_PORT);
//     if (lwip_connect(sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) != 0) {
//         printf("Error: Connection to target failed\n");
//         lwip_close(sock);
//         return -4;
//     }

//     // Format the standard HTTP/1.1 GET request string
//     char request_buffer[256];
//     int request_len = snprintf(request_buffer, sizeof(request_buffer),
//                                "GET %s HTTP/1.1\r\n"
//                                "Host: %s\r\n"
//                                "Connection: close\r\n"
//                                "\r\n",
//                                path, ip_str);

//     if (request_len < 0 || request_len >= (int)sizeof(request_buffer)) {
//         printf("Error: Request string truncated\n");
//         lwip_close(sock);
//         return -5;
//     }

//     // Transmit the HTTP request
//     printf("Sending HTTP GET request...\n");
//     int bytes_sent = lwip_write(sock, request_buffer, request_len);
//     if (bytes_sent < 0) {
//         printf("Error: Failed to write to socket\n");
//         lwip_close(sock);
//         return -6;
//     }

//     // Read the incoming response streams sequentially into the buffer
//     size_t total_bytes_received = 0;
//     int bytes_read = 0;

//     printf("Reading response...\n");
//     while (total_bytes_received < (response_sz - 1)) {
//         size_t remaining_space = response_sz - total_bytes_received - 1;
//         bytes_read = lwip_read(sock, response + total_bytes_received, remaining_space);

//         if (bytes_read < 0) {
//             printf("Error: Socket read error occurred\n");
//             lwip_close(sock);
//             return -7;
//         } else if (bytes_read == 0) {
//             // Server closed connection (Normal termination for Connection: close)
//             break;
//         }

//         total_bytes_received += bytes_read;
//     }

//     // Null-terminate the string safely inside the buffer boundaries
//     response[total_bytes_received] = '\0';

//     // Clean up the socket resource
//     lwip_close(sock);
//     return (int)total_bytes_received;
// }

/**
 * @brief Performs an HTTP GET request to the specified target IP and path.
 * 
 * @param target_addr Pointer to the native lwIP IP address structure.
 * @param path        The URI path (e.g., "/shelly").
 * @param response    Pointer to the buffer where the response will be stored.
 * @param response_sz The maximum size of the response buffer.
 * @param http_status Pointer to an integer where the HTTP status code will be placed.
 * @return int        The number of bytes received, or a negative error code on failure.
 */
int send_http_get(const ip_addr_t *target_addr, const char *path, char *response, size_t response_sz, int *http_status) 
{
    if (target_addr == NULL || response == NULL || response_sz == 0) {
        return -1;
    }

    // Initialize status code to 0 in case parsing fails or an error occurs early
    if (http_status != NULL) {
        *http_status = 0;
    }

    struct sockaddr_in remote_addr;
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = lwip_htons(HTTP_PORT);

    // Map lwIP's native ip_addr_t directly into the BSD socket sockaddr structure
    #if LWIP_IPV4
    remote_addr.sin_addr.s_addr = ip4_addr_get_u32(ip_2_ip4(target_addr));
    #else
    remote_addr.sin_addr.s_addr = target_addr->addr;
    #endif

    // Allocate a stream (TCP) socket
    int sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Error: Failed to create socket\n");
        return -3;
    }

    // Convert IP address back to string for display and the HTTP Host header
    char ip_str[16];
    ipaddr_ntoa_r(target_addr, ip_str, sizeof(ip_str));

    printf("Connecting to Shelly device at %s:%d...\n", ip_str, HTTP_PORT);
    if (lwip_connect(sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) != 0) {
        printf("Error: Connection to target failed\n");
        lwip_close(sock);
        return -4;
    }

    // Format the standard HTTP/1.1 GET request string
    char request_buffer[256];
    int request_len = snprintf(request_buffer, sizeof(request_buffer),
                               "GET %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "Connection: close\r\n"
                               "\r\n",
                               path, ip_str);

    if (request_len < 0 || request_len >= (int)sizeof(request_buffer)) {
        printf("Error: Request string truncated\n");
        lwip_close(sock);
        return -5;
    }

    // Transmit the HTTP request
    printf("Sending HTTP GET request...\n");
    int bytes_sent = lwip_write(sock, request_buffer, request_len);
    if (bytes_sent < 0) {
        printf("Error: Failed to write to socket\n");
        lwip_close(sock);
        return -6;
    }

    // Read the incoming response streams sequentially into the buffer
    size_t total_bytes_received = 0;
    int bytes_read = 0;

    printf("Reading response...\n");
    while (total_bytes_received < (response_sz - 1)) {
        size_t remaining_space = response_sz - total_bytes_received - 1;
        bytes_read = lwip_read(sock, response + total_bytes_received, remaining_space);

        if (bytes_read < 0) {
            printf("Error: Socket read error occurred\n");
            lwip_close(sock);
            return -7;
        } else if (bytes_read == 0) {
            // Server closed connection (Normal termination for Connection: close)
            break;
        }

        total_bytes_received += bytes_read;
    }

    // Null-terminate the string safely inside the buffer boundaries
    response[total_bytes_received] = '\0';

    // Parse the HTTP status code from the start of the response line (e.g., "HTTP/1.1 200 OK")
    if (total_bytes_received > 0 && http_status != NULL) {
        int parsed_status = 0;
        // %*d skips the minor version number, then we capture the integer status code
        if (sscanf(response, "HTTP/1.%*d %d", &parsed_status) == 1) {
            *http_status = parsed_status;
        } else {
            *http_status = -1; // Indicates failed parsing of the HTTP response status line
        }
    }

    // Clean up the socket resource
    lwip_close(sock);
    return (int)total_bytes_received;
}




//#include "lwip/apps/mdns_priv.h" // Requires internal lwIP mDNS headers

/**
 * Extracts a clean string from a raw mDNS packet buffer
 */
u16_t parse_mdns_name_client(struct pbuf *p, u16_t offset, char *dest, u16_t dest_len) {
    u8_t label_len;
    u16_t dest_idx = 0;
    u16_t original_offset = 0;
    int compressed = 0;
    
    while (1) {
        pbuf_copy_partial(p, &label_len, 1, offset);
        
        // 1. Check for standard End of Name (Root Domain)
        if (label_len == 0) {
            offset++; // Consume the 0x00 byte
            break; 
        }
        
        // 2. Check for DNS Pointer Compression (0xC0)
        if ((label_len & 0xC0) == 0xC0) {
            u8_t pointer_low_byte;
            pbuf_copy_partial(p, &pointer_low_byte, 1, offset + 1);
            
            // Calculate the jump destination target
            u16_t target_offset = ((label_len & 0x3F) << 8) | pointer_low_byte;
            
            // Save our current spot to return to later, but only the first time we jump
            if (!compressed) {
                original_offset = offset + 2; 
                compressed = 1;
            }
            
            // Jump the cursor to the compressed string location
            offset = target_offset;
            continue; 
        }
        
        // 3. Normal label processing
        offset++;
        if (dest_idx + label_len + 1 > dest_len) return 0; // Overflow safety
        
        pbuf_copy_partial(p, &dest[dest_idx], label_len, offset);
        offset += label_len;
        dest_idx += label_len;
        
        dest[dest_idx++] = '.';
    }
    
    // Clean up trailing dot
    if (dest_idx > 0) dest[dest_idx - 1] = '\0';
    
    // IF we jumped, the next record starts after the pointer bytes in the original location
    // IF we didn't jump, the next record starts right after our terminating 0x00 byte
    return compressed ? original_offset : offset;
}


/**
 * Loops through the buffer and extracts only the full names
 */
void extract_all_names(struct pbuf *p) 
{
    u16_t offset = 12; // Start right after the 12-byte header
    u16_t num_answers = 6; // Extracted from header bytes 6-7 (0x0006)
    
    char name_buffer[256];
    
    for (u16_t i = 0; i < num_answers; i++) {
        // 1. Extract the clean full name string
        u16_t next_offset = parse_mdns_name_client(p, offset, name_buffer, sizeof(name_buffer));
        
        if (next_offset == 0) {
            // Buffer parsing error safety catch
            break; 
        }
        
        // Print your clean extracted string
        printf("Record %d Full Name: %s\n", i + 1, name_buffer);
        
        // 2. Skip over the metadata to reach the next record's name
        // We need to read the 2-byte Data Length (RDLENGTH) field which is 8 bytes after the name ends
        u16_t rdlen_offset = next_offset + 8; 
        u16_t rdlen = 0;
        
        // Copy the 2-byte payload data length
        pbuf_copy_partial(p, &rdlen, 2, rdlen_offset);
        rdlen = lwip_ntohs(rdlen); // Ensure correct endianness conversion if required by architecture
        
        // Advance your cursor to the start of the next record
        // (next_offset + 10 bytes of fixed fields + size of the payload data)
        offset = next_offset + 10 + rdlen; 
    }
}

/**
 * Loops through the buffer and extracts only the full names
 */
int name_containing_string(struct pbuf *p, char *string_to_match) 
{
    u16_t offset = 12; // Start right after the 12-byte header
    u16_t num_answers = 6; // Extracted from header bytes 6-7 (0x0006)
    int present = 0;
    char name_buffer[256];
    
    for (u16_t i = 0; i < num_answers; i++) {
        // 1. Extract the clean full name string
        u16_t next_offset = parse_mdns_name_client(p, offset, name_buffer, sizeof(name_buffer));
        
        if (next_offset == 0) {
            // Buffer parsing error safety catch
            break; 
        }
        
        if (strstr(name_buffer, string_to_match) != NULL)
        {
            present = 1;
            break;
        }

        // Print your clean extracted string
        // printf("Record %d Full Name: %s\n", i + 1, name_buffer);
        
        // 2. Skip over the metadata to reach the next record's name
        // We need to read the 2-byte Data Length (RDLENGTH) field which is 8 bytes after the name ends
        u16_t rdlen_offset = next_offset + 8; 
        u16_t rdlen = 0;
        
        // Copy the 2-byte payload data length
        pbuf_copy_partial(p, &rdlen, 2, rdlen_offset);
        rdlen = lwip_ntohs(rdlen); // Ensure correct endianness conversion if required by architecture
        
        // Advance your cursor to the start of the next record
        // (next_offset + 10 bytes of fixed fields + size of the payload data)
        offset = next_offset + 10 + rdlen; 
    }

    return(present);
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