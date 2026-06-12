
/**
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 //#define _GNU_SOURCE
 

#include "lwip/apps/httpd.h"
#include <lwip/apps/fs.h>
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "calendar.h"
#include "config.h"
#include "pluto.h"
#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "hardware/watchdog.h"
#include "lwip/apps/httpd.h"
#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "flash.h"
#include "calendar.h"
#include "config.h"
#include "worker_tasks.h"
#include "hc_task.h"

// Pre-formatted valid HTTP response payload including minimalist JSON body data
const char http_200_json_response[] = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 15\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";

// Pre-allocated static raw response payloads stored in system flash memory
const char http_200_ok_json[] = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 15\r\n"
    "Connection: close\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";

const char http_400_bad_json[] = 
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 19\r\n"
    "Connection: close\r\n"
    "\r\n"
    "{\"status\":\"invalid\"}";    

// header used to send ascii buffer
const char http_ascii_buffer_header_tmpl[] = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain; charset=UTF-8\r\n"
    "Content-Length: %d\r\n"
    "Connection: close\r\n"
    "Access-Control-Allow-Origin: *\r\n" // Helps if testing locally via file://
    "\r\n";

#define MAX_ASCII_BUFFER_SIZE (4096) 
#define ASCII_HEADER_SIZE (sizeof(http_ascii_buffer_header_tmpl) - 1) 

// Application RAM Storage variables
char ascii_ram_buffer[ASCII_HEADER_SIZE + MAX_ASCII_BUFFER_SIZE];
size_t current_buffer_index = 0;

// Global flag to change filesystem responses context-dependently
bool post_validation_success = true;
const char *basic_program = ascii_ram_buffer + ASCII_HEADER_SIZE;


/**
 * @brief Decodes a URL-encoded string in-place.
 * @param src Pointer to the null-terminated string to decode.
 * @return Size of the decoded payload string in bytes.
 */
size_t url_decode_inplace(char *src) {
    char *dst = src;
    size_t length = 0;

    while (*src) {
        if (*src == '%') {
            // Check if there are at least 2 characters left to decode
            if (src[1] && src[2]) {
                char hex[3] = { src[1], src[2], '\0' };
                // Convert hex string snippet directly to a single byte character
                *dst = (char)strtol(hex, NULL, 16);
                src += 3;
            } else {
                // Malformed percent sign at the end of the string, copy as-is
                *dst = *src;
                src++;
            }
        } else if (*src == '+') {
            // HTML forms encode space characters as '+'
            *dst = ' ';
            src++;
        } else {
            *dst = *src;
            src++;
        }
        dst++;
        length++;
    }
    *dst = '\0'; // Ensure valid string termination in RAM
    return length;
}



err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       u16_t http_request_len, int content_len, char *response_uri,
                       u16_t response_uri_len, u8_t *post_auto_wnd) {
    
    if (strcmp(uri, "/save_ascii.cgi") == 0) {
        // Clear old working workspace before accepting new streaming content
        memset(ASCII_HEADER_SIZE + ascii_ram_buffer, 0, sizeof(ascii_ram_buffer) - ASCII_HEADER_SIZE);
        strcpy(ascii_ram_buffer, http_ascii_buffer_header_tmpl);  // prepend http header to ascii ram buffer
        current_buffer_index = 0;
        
        *post_auto_wnd = 1; // Direct lwIP to automatically manage TCP windows
        return ERR_OK;
    }
    return ERR_VAL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    struct pbuf *q;
    
    if (p == NULL) return ERR_OK;

    // Loop through the current pbuf packet chains
    for (q = p; q != NULL; q = q->next) {
        // Enforce safety limits to stop memory corruption overwrites
        if (current_buffer_index + q->len < (MAX_ASCII_BUFFER_SIZE - 1)) {
            memcpy(&ascii_ram_buffer[ASCII_HEADER_SIZE + current_buffer_index], q->payload, q->len);
            current_buffer_index += q->len;
        } else {
            // Payload is too large for the allocated RAM buffer
            pbuf_free(p);
            return ERR_MEM;
        }
    }

    pbuf_free(p); // Free packet memory back to the lwIP pool
    return ERR_OK;
}


/**
 * @brief Validates that a string is strictly 7-bit printable ASCII and adheres to line limits.
 * @param str The null-terminated string to validate.
 * @return true if valid, false if malicious/corrupt characters or line overflows are found.
 */
bool validate_ascii_buffer(const char *str) {
    size_t current_line_len = 0;

    while (*str != '\0') {
        char c = *str;

        if (c == '\n') {
            // Line break reset
            current_line_len = 0;
        } else {
            // Check for printable ASCII bounds (Space 0x20 to Tilde 0x7E)
            // Block structural escape codes like '\r', '\t', or '\0' derivatives
            if (c < 32 || c > 126) {
                return false; // Invalid hidden binary data or bad formatting detected
            }

            current_line_len++;
            
            // Hard safety fallback checkpoint on the MCU side
            if (current_line_len > 80) {
                return false; // Strict boundary violation
            }
        }
        str++;
    }
    return true; 
}


void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
    ascii_ram_buffer[ASCII_HEADER_SIZE + current_buffer_index] = '\0';
    post_validation_success = false; // Default to fail until validated

    char *data_start = strstr(ASCII_HEADER_SIZE + ascii_ram_buffer, "text=");
    if (data_start != NULL) {
        data_start += 5; // Move past "text=" Prefix key
        
        // 1. In-place URL-decode the string
        size_t final_len = url_decode_inplace(data_start);
        memmove(ASCII_HEADER_SIZE + ascii_ram_buffer, data_start, final_len + 1);

        // 2. Perform the character and layout integrity validation check
        if (validate_ascii_buffer(ASCII_HEADER_SIZE + ascii_ram_buffer)) {
            post_validation_success = true;
            // The sanitized string is now safe to process or use in RAM here
        }
    }

    // 3. Select virtual response path based on validation results
    if (post_validation_success) {
        snprintf(response_uri, response_uri_len, "/post_ok.json");
    } else {
        snprintf(response_uri, response_uri_len, "/post_fail.json");
    }
}


int fs_open_custom(struct fs_file *file, const char *name) {
    // Intercept target paths completely bypassing standard internal file matching
    if (strcmp(name, "/post_ok.json") == 0) {
        memset(file, 0, sizeof(struct fs_file));
        file->data = http_200_ok_json;
        file->len  = sizeof(http_200_ok_json) - 1;
        file->index = sizeof(http_200_ok_json) - 1;

        // Tell lwIP's HTTPD state machine that headers are accounted for
        // (This stops the "HTTP headers not included in file system" crash)
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;
        return 1;
    }
    
    if (strcmp(name, "/post_fail.json") == 0) {
        memset(file, 0, sizeof(struct fs_file));
        file->data = http_400_bad_json;
        file->len  = sizeof(http_400_bad_json) - 1;
        file->index = sizeof(http_400_bad_json) - 1;

        // Tell lwIP's HTTPD state machine that headers are accounted for
        // (This stops the "HTTP headers not included in file system" crash)
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;
        return 1;
    }

    if (strcmp(name, "/get_text") == 0) {
        size_t len = strlen(ascii_ram_buffer);
        file->data = ascii_ram_buffer;
        file->len = len;
        file->index = len;
        //file->pextension = NULL;
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;
        return 1;
    }    

    return 0; // Hand back control to default storage for regular SSI/CGI scripts
}

/**
 * @brief Callback executed by lwIP HTTPD when a custom file is closed.
 * @param file Pointer to the lwIP fs_file structure being torn down.
 */
void fs_close_custom(struct fs_file *file)
{
    // trigger the hasic interpreter to run every time buffer is sent by the browser
    hc_queue_send(69);

}

void dump_text_buffer(void)
{
    int i;

    for(i=0; i < MAX_ASCII_BUFFER_SIZE; i++)
    {
        if(!(i%80)) printf("\n");

        printf("%c", ascii_ram_buffer[i]);        
    }
    
}