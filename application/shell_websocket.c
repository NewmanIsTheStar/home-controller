// 1. ALWAYS force lwip/opt.h to load first. 
// This internally pulls your local 'lwipopts.h' configuration correctly.
#include "lwip/opt.h"

// 2. Safely verify that your macro overrides have taken effect
#if !LWIP_HTTPD_SUPPORT_WEBSOCKET
#error "LWIP_HTTPD_SUPPORT_WEBSOCKET must be enabled in lwipopts.h to use this file!"
#endif

#include "lwip/apps/httpd.h"
#include "lwip/tcp.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "pico/cyw43_arch.h"
#include <string.h>

#define MAX_WS_LEN 128

// The correct macro value for a text frame according to the WebSocket protocol RFC
#define WS_OPCODE_TEXT 0x01

// Global pointer tracking the live web client socket link safely
static struct tcp_pcb *active_ws_pcb = NULL; 

/**
 * Global function to push text to the browser at ANY point asynchronously.
 * Manually packages the raw string into a conformant WebSocket frame over native TCP.
 */
void ws_send_async_text(const char* text) {
    if (active_ws_pcb != NULL && text != NULL) {
        size_t text_len = strlen(text);
        if (text_len > 65535) return; // Cap safety length limit for simple 16-bit payload framing

        uint8_t header[4];
        size_t header_len = 0;

        // Byte 0: Fin bit set (0x80) | Opcode text (0x01) -> 0x81
        header[header_len++] = 0x80 | WS_OPCODE_TEXT;

        // Byte 1+: Payload length configuration (Server-to-client frames MUST NOT be masked)
        if (text_len < 126) {
            header[header_len++] = (uint8_t)text_len;
        } else {
            header[header_len++] = 126;
            header[header_len++] = (uint8_t)((text_len >> 8) & 0xFF);
            header[header_len++] = (uint8_t)(text_len & 0xFF);
        }

        // Stage frame components inside the raw lwIP outbound ring buffer
        tcp_write(active_ws_pcb, header, header_len, TCP_WRITE_FLAG_COPY);
        tcp_write(active_ws_pcb, text, (uint16_t)text_len, TCP_WRITE_FLAG_COPY);
        
        // Push packet immediately down the wire
        tcp_output(active_ws_pcb);
    }
}

// Shell logic router 
static void process_ws_command(const char* cmd) {
    char response[256];
    
    if (strcmp(cmd, "help") == 0) {
        snprintf(response, sizeof(response), "Pico 2 W commands:\n  help   - Show menu\n  async  - Trigger async message\n");
        ws_send_async_text(response);
    } else if (strcmp(cmd, "async") == 0) {
        ws_send_async_text("Command recognized. Triggering background work...\n");
        ws_send_async_text("[ASYNC ALERT] Event dispatched asynchronously from the Pico background timer loop!\n");
    } else {
        snprintf(response, sizeof(response), "Unknown Command: '%s'\n", cmd);
        ws_send_async_text(response);
    }
}

/**
 * 1. Handshake Initialized
 */
err_t websocket_open_cb(struct tcp_pcb *pcb, const char *uri) {
    if (strcmp(uri, "/ws") == 0) {
        active_ws_pcb = pcb; 
        return ERR_OK;
    }
    return ERR_VAL;
}

/**
 * 2. Inbound WebSocket Data Frame Processor
 */
void websocket_receive_cb(struct tcp_pcb *pcb, uint8_t flags, uint8_t *payload, uint16_t tot_len) {
    uint8_t opcode = flags & 0x0F;
    
    if (opcode == WS_OPCODE_TEXT && tot_len > 0) {
        char cmd_in[MAX_WS_LEN];
        uint16_t copy_len = (tot_len < MAX_WS_LEN - 1) ? tot_len : MAX_WS_LEN - 1;
        
        memcpy(cmd_in, payload, copy_len);
        cmd_in[copy_len] = '\0';
        
        process_ws_command(cmd_in);
    }
}

/**
 * 3. Connection Terminated
 */
void websocket_closed_cb(struct tcp_pcb *pcb) {
    if (active_ws_pcb == pcb) {
        active_ws_pcb = NULL; 
    }
}

/**
 * Hook initializer
 */
void init_websocket_subsystem(void) {
    //httpd_register_websocket_callbacks(websocket_open_cb, websocket_receive_cb, websocket_closed_cb);
}
