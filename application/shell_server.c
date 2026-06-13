#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include <string.h>

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

#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include <string.h>

// #include "lwip/apps/httpd.h"
// #include "lwip/apps/fs.h"
// #include "lwip/def.h"
// #include "lwip/mem.h"
// #include "pico/cyw43_arch.h"
// #include <string.h>

#define MAX_CMD_LEN  128
#define RESP_BUF_LEN 512

static char cmd_buffer[MAX_CMD_LEN];
static uint16_t cmd_len = 0;

// Dynamic buffer to hold the response data for the browser
static char response_data[RESP_BUF_LEN];
static int response_len = 0;

static void execute_command(const char* cmd, char* out_buf, size_t max_len) {
    int i;
    if (strcmp(cmd, "help") == 0) {
        snprintf(out_buf, max_len, "Pico 2 W commands:\n  help   - Show menu\n  led_on  - Turn on onboard LED\n  led_off - Turn off onboard LED\n");
    } else if (strcmp(cmd, "led_on") == 0) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        snprintf(out_buf, max_len, "Onboard LED is now ON\n");
    } else if (strcmp(cmd, "led_off") == 0) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        snprintf(out_buf, max_len, "Onboard LED is now OFF\n");
    } else if (strcmp(cmd, "limit") == 0) {   
        snprintf(out_buf, max_len, "max output string lenght is %d\n", max_len);
        for(i=0; i< 1000; i++)
        {
            
        }
    } else {
        snprintf(out_buf, max_len, "Unknown command: '%s'\n", cmd);
    }
}

// 1. Called when the browser issues POST /exec
err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       uint16_t http_request_len, int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd) {
    if (strcmp(uri, "/exec") == 0) {
        cmd_len = 0;
        memset(cmd_buffer, 0, MAX_CMD_LEN);
        *post_auto_wnd = 1;
        return ERR_OK;
    }
    return ERR_VAL;
}

// 2. Called as payload packets stream in
err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    if (p != NULL) {
        if (cmd_len + p->tot_len < MAX_CMD_LEN - 1) {
            pbuf_copy_partial(p, cmd_buffer + cmd_len, p->tot_len, 0);
            cmd_len += p->tot_len;
            cmd_buffer[cmd_len] = '\0';
        }
        pbuf_free(p);
    }
    return ERR_OK;
}

// 3. Called when POST data ingestion is complete
void httpd_post_finished(void *connection, char *response_uri, uint16_t response_uri_len) {
    // Process the shell command and fill our response memory buffer
    memset(response_data, 0, RESP_BUF_LEN);
    execute_command(cmd_buffer, response_data, RESP_BUF_LEN);
    response_len = strlen(response_data);

    // Direct lwIP to a virtual response filename path
    snprintf(response_uri, response_uri_len, "/response.txt");
}

/**
 * Custom File System Hooks
 * Intercepts lwIP's internal file system layer to serve our memory buffer.
 */
int fs_open_custom(struct fs_file *file, const char *name) {
    // When lwIP looks to read the response file path set in httpd_post_finished
    if (strcmp(name, "/response.txt") == 0) {
        file->data = response_data;
        //file->pextension = NULL;
        file->index = response_len;
        file->len = response_len;
        
        // Mark file flags to denote that HTTP headers must be appended dynamically
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED; 
        
        // Manually prepend headers to the data chunk safely
        static char dynamic_http_payload[RESP_BUF_LEN + 128];
        int header_len = snprintf(dynamic_http_payload, sizeof(dynamic_http_payload),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n\r\n"
            "%s", response_len, response_data);
            
        file->data = dynamic_http_payload;
        file->index = header_len;
        file->len = header_len;
        
        return 1; // File found/handled successfully
    }
    return 0; // Fall back to standard flash file system storage (`makefsdata`)
}

void fs_close_custom(struct fs_file *file) {
    // Custom clean-up logic goes here if allocating dynamic allocations per connection
}

