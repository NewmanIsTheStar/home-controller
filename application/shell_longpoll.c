#include "lwip/opt.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "pico/cyw43_arch.h"
#include <string.h>
#include "hc_task.h"
#include "pluto.h"

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
    } else {
        hc_load_basic_program((char *)cmd, strlen(cmd));
        hc_queue_send(67);        
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

    return 0;
}

void fs_close_custom(struct fs_file *file) {
    if (pending_listen_file == file) {
        pending_listen_file = NULL;
    }
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
        return ERR_OK;
    }
    return ERR_VAL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
    if (p != NULL) {
        if (post_len + p->tot_len < sizeof(post_buffer) - 1) {
            pbuf_copy_partial(p, post_buffer + post_len, p->tot_len, 0);
            post_len += p->tot_len;
            post_buffer[post_len] = '\0';
        }
        pbuf_free(p);
    }
    return ERR_OK;
}

void httpd_post_finished(void *connection, char *response_uri, uint16_t response_uri_len) {
    snprintf(response_uri, response_uri_len, "/post_ok.txt");
    execute_shell_command(post_buffer);
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
