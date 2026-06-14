#include "lwip/opt.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "pico/cyw43_arch.h"
#include <string.h>
#include "hc_task.h"
#include "pluto.h"

typedef enum
{
    HTTP_RX_UNKNOWN,
    HTTP_RX_EXEC,
    HTTP_RX_SAVE_ASCII
} HTTP_RX_TYPE_T;

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

#define ITEM_BUF_LEN 128
#define QUEUE_DEPTH  50   // Can store up to 8 messages simultaneously

// A larger temporary compilation buffer to merge multiple messages together safely
#define BULK_SEND_BUF_LEN (QUEUE_DEPTH * ITEM_BUF_LEN)
static char bulk_http_payload[BULK_SEND_BUF_LEN + 128];

// Thread-safe circular ring buffer storage structures
static char queue_storage[QUEUE_DEPTH][ITEM_BUF_LEN];
static uint16_t queue_lengths[QUEUE_DEPTH];
static volatile int queue_head = 0;
static volatile int queue_tail = 0;
static struct fs_file *pending_listen_file = NULL;
static struct fs_file *pending_get_text = NULL;

// Application RAM Storage variables
char ascii_ram_buffer[ASCII_HEADER_SIZE + MAX_ASCII_BUFFER_SIZE];
size_t current_buffer_index = 0;
bool post_validation_success = true;
const char *basic_program = ascii_ram_buffer + ASCII_HEADER_SIZE;
HTTP_RX_TYPE_T current_post = HTTP_RX_UNKNOWN;

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

// Helper function to check if queue is empty
static inline int queue_is_empty(void) {
    return queue_head == queue_tail;
}

// Helper function to check if queue is full
static inline int queue_is_full(void) {
    return ((queue_head + 1) % QUEUE_DEPTH) == queue_tail;
}

/**
 * Optimized Flush Function.
 * Drains ALL queued items simultaneously into a single chunk response frame.
 */
static void check_and_flush_queue(void) {
    // Only proceed if we have a browser actively waiting AND data queued up
    if (pending_listen_file != NULL && !queue_is_empty()) {
        
        // 1. Initialize a pointer to build our text payload body
        char text_body_buf[BULK_SEND_BUF_LEN];
        memset(text_body_buf, 0, sizeof(text_body_buf));
        size_t current_body_len = 0;

        // 2. Loop and completely drain the queue ring into our single string buffer
        while (!queue_is_empty()) {
            char* active_text = queue_storage[queue_tail];
            size_t active_len = queue_lengths[queue_tail];

            // Safety check to ensure we don't overflow the transient assembly block
            if (current_body_len + active_len < BULK_SEND_BUF_LEN - 1) {
                memcpy(text_body_buf + current_body_len, active_text, active_len);
                current_body_len += active_len;
            } else {
                // Buffer full; stop pulling items from queue for this cycle
                break;
            }

            // Safely advance tail index
            queue_tail = (queue_tail + 1) % QUEUE_DEPTH;
        }
        text_body_buf[current_body_len] = '\0';

        // 3. Format standard dynamic raw HTTP delivery wrapper package headers
        int total_len = snprintf(bulk_http_payload, sizeof(bulk_http_payload),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n\r\n"
            "%s", (int)current_body_len, text_body_buf);

        // 4. Update file pointer configuration to trigger immediate single-packet execution
        pending_listen_file->data = bulk_http_payload;
        pending_listen_file->len  = total_len;
        pending_listen_file->index = total_len;

        // Reset tracking descriptor so subsequent long-polls wait cleanly
        pending_listen_file = NULL;
    }
}

/**
 * Public method to push text arrays asynchronously.
 * Fully decoupled from immediate network delivery to prevent data loss.
 */
void pico_send_async_text(const char* text) {
    if (text == NULL) return;

    // Guard against potential thread concurrency collisions
    cyw43_arch_lwip_begin();

    if (!queue_is_full()) {
        // Copy string payload safely into current head index
        memset(queue_storage[queue_head], 0, ITEM_BUF_LEN);
        snprintf(queue_storage[queue_head], ITEM_BUF_LEN, "%s", text);
        queue_lengths[queue_head] = strlen(queue_storage[queue_head]);

        // Shift head circular tracking index ahead
        queue_head = (queue_head + 1) % QUEUE_DEPTH;

        // Instantly verify if a browser connection is waiting to take this item
        check_and_flush_queue();
    } else {
        // Optional: Log an internal overflow drop event error here if buffer size isn't deep enough
    }

    cyw43_arch_lwip_end();
}

// Shell command processor
static void execute_shell_command(const char* cmd) {
    char reply[ITEM_BUF_LEN];
    if (strcmp(cmd, "help") == 0) {
        snprintf(reply, sizeof(reply), "Pico 2 W Options:\n  help  - Show options\n  ping  - Check response\n");
        pico_send_async_text(reply);
    } else if (strcmp(cmd, "ping") == 0) {
        // Example: Sending multiple packets sequentially without waiting
        pico_send_async_text("pong line 1!\n");
        pico_send_async_text("pong line 2!\n");
        pico_send_async_text("pong line 3!\n");
    } else if (strcmp(cmd, "lights") == 0) {
        // Example: Sending multiple packets sequentially without waiting
        hc_queue_send(HC_CMD_LIGHTS);  
    } else {
        hc_load_basic_program((char *)cmd, strlen(cmd));
        hc_queue_send(HC_CMD_BASIC_INTERACTIVE);        
        // snprintf(reply, sizeof(reply), "Unknown command: '%s'\n", cmd);
        // pico_send_async_text(reply);
    }
}

/**
 * Custom File System Override Hooks
 */
int fs_open_custom(struct fs_file *file, const char *name) {
    if (strcmp(name, "/listen.shtml") == 0) {
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        //file->pextension = NULL;

        // Cache file pointer structure ahead of loop test parsing
        pending_listen_file = file;

        // If data is already sitting in the queue ring, compile and flush instantly
        if (!queue_is_empty()) {
            check_and_flush_queue();
            return 1;
        }

        // No data ready yet. Stall request safely using placeholder non-null pointer address
        static char empty_placeholder = ' '; 
        file->data = &empty_placeholder; 
        file->len = 0;
        file->index = 0;
        return 1; 
    }

    if (strcmp(name, "/post_ok.txt") == 0) {
        static const char *ok_resp = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
        file->data = (char *)ok_resp;
        file->len = strlen(ok_resp);
        file->index = strlen(ok_resp);
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        //file->pextension = NULL;
        return 1;
    }

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

        // Cache file pointer structure ahead of loop test parsing
        pending_get_text = file;        
        return 1;
    }      

    return 0;
}

void fs_close_custom(struct fs_file *file) {
    if (pending_listen_file == file) {
        pending_listen_file = NULL;
    }

    // if (pending_get_text == file) {
    //     pending_get_text = NULL;
    //     hc_queue_send(HC_CMD_BASIC_SCRIPT);        
    // }

}



/**
 * Standard HTTP POST Handlers for /exec commands
 */
static char post_buffer[ITEM_BUF_LEN];
static uint16_t post_len = 0;

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       uint16_t http_request_len, int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd) {
    if (strcmp(uri, "/exec") == 0) {
        post_len = 0;
        memset(post_buffer, 0, sizeof(post_buffer));
        *post_auto_wnd = 1;

        current_post = HTTP_RX_EXEC;
        return ERR_OK;
    }

    if (strcmp(uri, "/save_ascii.cgi") == 0) {
        // Clear old working workspace before accepting new streaming content
        memset(ASCII_HEADER_SIZE + ascii_ram_buffer, 0, sizeof(ascii_ram_buffer) - ASCII_HEADER_SIZE);
        strcpy(ascii_ram_buffer, http_ascii_buffer_header_tmpl);  // prepend http header to ascii ram buffer
        current_buffer_index = 0;
        *post_auto_wnd = 1; // Direct lwIP to automatically manage TCP windows

        current_post = HTTP_RX_SAVE_ASCII;
        return ERR_OK;
    }    
    return ERR_VAL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) 
{
    struct pbuf *q;

    switch(current_post)
    {
    default:
    case HTTP_RX_EXEC:
        if (p != NULL) 
        {
            if (post_len + p->tot_len < sizeof(post_buffer) - 1) 
            {
                pbuf_copy_partial(p, post_buffer + post_len, p->tot_len, 0);
                post_len += p->tot_len;
                post_buffer[post_len] = '\0';
            }
            pbuf_free(p);
        }    
        break;
    case HTTP_RX_SAVE_ASCII:
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
        break;
    }

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

void httpd_post_finished(void *connection, char *response_uri, uint16_t response_uri_len) 
{
    switch(current_post)
    {
    default:
    case HTTP_RX_EXEC:    
        snprintf(response_uri, response_uri_len, "/post_ok.txt");
        execute_shell_command(post_buffer);
        break;
    case HTTP_RX_SAVE_ASCII:
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
            hc_queue_send(HC_CMD_BASIC_SCRIPT);  
        } else {
            snprintf(response_uri, response_uri_len, "/post_fail.json");
        }    
        break;
    }
}

void init_shell_backend(void) {
    // Hooks initialized implicitly via file architecture layer structures
}

/**
 * Blocking method to print to browser 
 */
void shell_print(const char* text) 
{
    int retry = 0;
    bool complete = false;

    if (text == NULL) return;

    do
    {
        // Guard against potential thread concurrency collisions
        cyw43_arch_lwip_begin();

        if (!queue_is_full()) 
        {
            // Copy string payload safely into current head index
            memset(queue_storage[queue_head], 0, ITEM_BUF_LEN);
            snprintf(queue_storage[queue_head], ITEM_BUF_LEN, "%s", text);
            queue_lengths[queue_head] = strlen(queue_storage[queue_head]);

            // Shift head circular tracking index ahead
            queue_head = (queue_head + 1) % QUEUE_DEPTH;

            // Instantly verify if a browser connection is waiting to take this item
            check_and_flush_queue();
            complete = true;
            cyw43_arch_lwip_end();            
        } else
        {
            // Optional: Log an internal overflow drop event error here if buffer size isn't deep enough
            cyw43_arch_lwip_end();
            SLEEP_MS(10);
        }

        //cyw43_arch_lwip_end(); DANGER! Make sure we always release the lock!!!

        retry++;
    } while (!complete && retry < 200);
    
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