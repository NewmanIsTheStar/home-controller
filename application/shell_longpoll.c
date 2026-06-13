#include "lwip/opt.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "pico/cyw43_arch.h"
#include <string.h>
#include "hc_task.h"

#define RESP_BUF_LEN 256

extern const char *basic_program;

// Storage buffers for our long poll text
static char async_text_buffer[RESP_BUF_LEN];
static int async_text_len = 0;
static uint8_t data_is_ready = 0;

// Keep track of the file structure that lwIP is waiting on
static struct fs_file *pending_listen_file = NULL;

/**
 * Public method to push string alerts asynchronously.
 * Can be safely called from your main loops or hardware timers.
 */
void pico_send_async_text(const char* text) {
    if (text == NULL) return;

    // Stage the data into our global buffer
    memset(async_text_buffer, 0, RESP_BUF_LEN);
    snprintf(async_text_buffer, RESP_BUF_LEN, "%s", text);
    async_text_len = strlen(async_text_buffer);
    data_is_ready = 1;

    // If a browser is actively waiting for data, force lwIP to process it now
    if (pending_listen_file != NULL) {
        // Prepare the standard HTTP raw delivery wrapper payload
        static char http_payload[RESP_BUF_LEN + 128];
        int total_len = snprintf(http_payload, sizeof(http_payload),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n\r\n"
            "%s", async_text_len, async_text_buffer);

        // Populate the pending file object structure with our dynamic string
        pending_listen_file->data = http_payload;
        pending_listen_file->len  = total_len;
        pending_listen_file->index = total_len;

        // Clear references so the next loop can register cleanly
        pending_listen_file = NULL;
        data_is_ready = 0;
    }
}

// Shell command processor
static void execute_shell_command(const char* cmd) {
    char reply[RESP_BUF_LEN];
    if (strcmp(cmd, "help") == 0) {
        snprintf(reply, sizeof(reply), "Pico 2 W Options:\n  help  - Show options\n  ping  - Check response\n");
        pico_send_async_text(reply);
    } else if (strcmp(cmd, "ping") == 0) {
        pico_send_async_text("pong!\n");
    } else { 
        hc_load_basic_program((char *)cmd, strlen(cmd));
        hc_queue_send(67);
        //snprintf(reply, sizeof(reply), "Unknown command: '%s'\n", cmd);
        //pico_send_async_text(reply);
    }
}

/**
 * Custom File System Override Hooks
 * Handles interception of web requests safely.
 */
int fs_open_custom(struct fs_file *file, const char *name) {
    // Intercept our custom /listen.shtml endpoint
    if (strcmp(name, "/listen.shtml") == 0) {
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        //file->pextension = NULL;

        // SCENARIO A: Data is already waiting in the buffer when the browser requests it
        if (data_is_ready) {
            static char http_payload[RESP_BUF_LEN + 128];
            int total_len = snprintf(http_payload, sizeof(http_payload),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n\r\n"
                "%s", async_text_len, async_text_buffer);

            file->data = http_payload;
            file->len = total_len;
            file->index = total_len;
            data_is_ready = 0;
            return 1;
        }

        // SCENARIO B: FIX -> Provide a valid non-NULL pointer placeholder but set length to 0.
        // This stops lwIP from asserting, but tells the TCP state engine there is 
        // absolutely nothing to transmit yet, successfully freezing the request line.
        static char empty_placeholder = ' '; 
        file->data = &empty_placeholder; 
        file->len = 0;
        file->index = 0;
        
        pending_listen_file = file;
        return 1; 
    }

    // Intercept our clean command confirmation target path
    if (strcmp(name, "/post_ok.txt") == 0) {
        static const char *ok_resp = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
        file->data = (char *)ok_resp;
        file->len = strlen(ok_resp);
        file->index = strlen(ok_resp);
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        //file->pextension = NULL;
        return 1;
    }

    return 0; // Fall back to regular flash memory asset delivery
}

void fs_close_custom(struct fs_file *file) {
    if (pending_listen_file == file) {
        pending_listen_file = NULL;
    }
}

/**
 * Standard HTTP POST Handling Structures for Inbound /exec commands
 */
static char post_buffer[128];
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
    // Return our blank 204 confirmation path instantly to finish the POST channel transaction
    snprintf(response_uri, response_uri_len, "/post_ok.txt");

    // Pass the typed command down to our core router execution engine
    execute_shell_command(post_buffer);
}

void init_shell_backend(void) {
    // No CGI table hooks required anymore. Custom file hooks take over everything.
}
