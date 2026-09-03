//#define _GNU_SOURCE 
#include <sys/stat.h>
#include <fcntl.h>

#include "lwip/opt.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "pico/cyw43_arch.h"
#include <string.h>
#include "hc_task.h"
#include "pluto.h"
#include "shell.h"
#include "ping_core.h"
#include <stdio.h>
//#include <unistd.h>
#include "picofs.h"

// defines
#define ITEM_BUF_LEN (128)
#define QUEUE_DEPTH  (50)   
#define BULK_SEND_BUF_LEN (QUEUE_DEPTH * ITEM_BUF_LEN)           


// typedefs
typedef enum
{
    HTTP_RX_UNKNOWN,
    HTTP_RX_EXEC,
    HTTP_RX_SAVE_ASCII
} HTTP_RX_TYPE_T;

// external variables
extern u32_t unix_time;
extern WEB_VARIABLES_T web;
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

// prototypes
int shell_edit(char *filename);
int shell_cat(char *filename);
void shell_printf_nb(const char *format, ...);
int shell_map_test(char *filename);


const char http_200_json_response[] = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 15\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";

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

// reference to constant requires this define to appear here
#define ASCII_HEADER_SIZE (sizeof(http_ascii_buffer_header_tmpl) - 1) 


// A larger temporary compilation buffer to merge multiple messages together safely
static char bulk_http_payload[BULK_SEND_BUF_LEN + 128];

// Thread-safe circular ring buffer storage structures
static char queue_storage[QUEUE_DEPTH][ITEM_BUF_LEN];
static uint16_t queue_lengths[QUEUE_DEPTH];
static volatile int queue_head = 0;
static volatile int queue_tail = 0;
static struct fs_file *pending_listen_file = NULL;
static struct fs_file *pending_get_text = NULL;

// Application RAM Storage variables
char tab_completion_buffer[1024];
char ascii_ram_buffer[ASCII_HEADER_SIZE + MAX_PROGRAM_SIZE];
size_t current_buffer_index = 0;
bool post_validation_success = true;
const char *basic_program = ascii_ram_buffer + ASCII_HEADER_SIZE;
const char *editor_text = ascii_ram_buffer + ASCII_HEADER_SIZE;
HTTP_RX_TYPE_T current_post = HTTP_RX_UNKNOWN;

static char post_buffer[ITEM_BUF_LEN];
static uint16_t post_len = 0;

// tracker variable for the filesystem state
extern int tab_completion_sequence; 
static char last_client_etag[16] = {0};

/*!
 * \brief Decodes a URL-encoded string in-place.
 *
 * \param[in]  src  pointer to the null-terminated string to decode.
 * 
 * \return size of the decoded payload string in bytes.
 */
size_t url_decode_inplace(char *src) 
{
    char *dst = src;
    size_t length = 0;

    while (*src) 
    {
        if (*src == '%') 
        {
            // Check if there are at least 2 characters left to decode
            if (src[1] && src[2]) 
            {
                char hex[3] = { src[1], src[2], '\0' };
                // Convert hex string snippet directly to a single byte character
                *dst = (char)strtol(hex, NULL, 16);
                src += 3;
            } 
            else 
            {
                // Malformed percent sign at the end of the string, copy as-is
                *dst = *src;
                src++;
            }
        } 
        else if (*src == '+') 
        {
            // HTML forms encode space characters as '+'
            *dst = ' ';
            src++;
        } 
        else 
        {
            *dst = *src;
            src++;
        }
        dst++;
        length++;
    }
    *dst = '\0'; // Ensure valid string termination in RAM
    return length;
}


/*!
 * \brief Helper function to check if queue is empty
 * 
 * \return 1 if queue is empty.
 */
static inline int queue_is_empty(void) 
{
    return queue_head == queue_tail;
}


/*!
 * \brief Helper function to check if queue is full
 * 
 * \return 1 if queue is full.
 */
static inline int queue_is_full(void) 
{
    return ((queue_head + 1) % QUEUE_DEPTH) == queue_tail;
}


/*!
 * \brief Drains ALL queued items simultaneously into a single chunk response frame.
 * 
 * \return nothing
 */
static void check_and_flush_queue(void) 
{
    // Only proceed if we have a browser actively waiting AND data queued up
    if (pending_listen_file != NULL && !queue_is_empty()) 
    {
        
        // 1. Initialize a pointer to build our text payload body
        char text_body_buf[BULK_SEND_BUF_LEN];
        memset(text_body_buf, 0, sizeof(text_body_buf));
        size_t current_body_len = 0;

        // 2. Loop and completely drain the queue ring into our single string buffer
        while (!queue_is_empty()) 
        {
            char* active_text = queue_storage[queue_tail];
            size_t active_len = queue_lengths[queue_tail];

            // Safety check to ensure we don't overflow the transient assembly block
            if (current_body_len + active_len < BULK_SEND_BUF_LEN - 1) 
            {
                memcpy(text_body_buf + current_body_len, active_text, active_len);
                current_body_len += active_len;
            } 
            else 
            {
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

/*!
 * \brief Push text arrays asynchronously. Fully decoupled from immediate network delivery to prevent data loss.
 *
 * \param[in]  text  pointer to the null-terminated string to send.
 * 
 * \return nothing
 */
void pico_send_async_text(const char* text) 
{
    if (text == NULL) return;

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
    } 
    else 
    {
        // Optional: Log an internal overflow drop event error here if buffer size isn't deep enough
    }

    cyw43_arch_lwip_end();
}

/*!
 * \brief process shell commands
 *
 * \param[in]  cmd  command line entered by user
 * 
 * \return nothing
 */
static void execute_shell_command(const char* cmd) 
{
    int i;
    char reply[ITEM_BUF_LEN];

    if (strcmp(cmd, "help") == 0) 
    {
        shell_printf_nb("help    - show options\n");
        shell_printf_nb("cat     - print text file\n");
        shell_printf_nb("cleanup - erase obsolete sectors of file system\n");
        shell_printf_nb("cp      - copy file\n");
        shell_printf_nb("date    - calendar time\n");
        shell_printf_nb("defrag  - defragment file system\n");
        shell_printf_nb("df      - show file system usage\n");
        shell_printf_nb("edit    - edit a text file\n");
        shell_printf_nb("hd      - hex dump\n");
        shell_printf_nb("ip      - network info\n");
        shell_printf_nb("ls      - list files\n");
        shell_printf_nb("mv      - move file\n");
        shell_printf_nb("ping    - check ip connectivity\n");
        shell_printf_nb("rm      - remove file\n");
        shell_printf_nb("run     - execute a basic script\n");
        shell_printf_nb("uptime  - time since boot\n");    
    }
    else if (strncmp(cmd, "ping ", 5) == 0)
    {        
        STRNCPY(web.ping_target, (char *)cmd+5, sizeof(web.ping_target));  // TODO: this is a kludge, should be passing this data in the queue 
        hc_queue_send(HC_CMD_PING);
    }     
    else if (strcmp(cmd, "ff") == 0)
    {
        #if FS_FAKE_FLASH == 1
        hex_dump((char *)test_filesystem, sizeof(test_filesystem));
        #endif

    }     
    else if (strcmp(cmd, "lights") == 0)
    {
        hc_queue_send(HC_CMD_LIGHTS);  
    } 
    else if (strcmp(cmd, "ls a") == 0) 
    {
        hc_queue_send(HC_CMD_LIST_CORRUPT);          
    } 
    else if (strcmp(cmd, "ls") == 0) 
    {
        hc_queue_send(HC_CMD_LIST);          
    }     
    else if (strcmp(cmd, "dump program") == 0) 
    {
        hc_queue_send(HC_CMD_DUMP_PROGRAM);  
    } 
    else if (strncmp(cmd, "shelly dump ", 12) == 0) 
    {
        if (strlen(cmd) > 13)
        {
            web.shelly_device_ip = ((char *)(cmd+12)); 
            hc_queue_send(HC_CMD_SHELLY_DEVICE_DUMP);  
        }
    }     
    else if (strncmp(cmd, "run ", 4) == 0) 
    {
        if (strlen(cmd) > 5)
        {
            web.basic_file_to_execute = ((char *)(cmd+4)); 
            hc_queue_send(HC_CMD_BASIC_FILE);  
        }
    }     
    else if (strcmp(cmd, "df") == 0) 
    {
        hc_queue_send(HC_CMD_PAGE_MAP);  
    }  
    else if (strcmp(cmd, "df n") == 0) 
    {
        hc_queue_send(HC_CMD_PAGE_NUMBERS);  
    }      
    else if (strcmp(cmd, "cleanup") == 0) 
    {
        hc_queue_send(HC_CMD_DISK_CLEANUP); 
    }      
    else if (strcmp(cmd, "defrag") == 0) 
    {
        hc_queue_send(HC_CMD_DEFRAGMENT); 
    }     
    else if (strncmp(cmd, "edit ", 5) == 0) 
    {
        if (strlen(cmd) > 6) shell_edit((char *)(cmd+5));  
    }                 
    else if (strncmp(cmd, "cat ", 4) == 0) 
    {
        web.file_to_cat = ((char *)(cmd+4)); 
        hc_queue_send(HC_CMD_CAT_FILE);  
    } 
    else if (strncmp(cmd, "hd ", 3) == 0) 
    {
        web.file_to_hexdump = ((char *)(cmd+3)); 
        hc_queue_send(HC_CMD_HEXDUMP_FILE);  
    }     
    // else if (strncmp(cmd, "hd ", 3) == 0) 
    // {        
    //     if (strlen(cmd) > 5) shell_hex_dump((char *)cmd+3);   
    // } 
    else if (strncmp(cmd, "mt ", 3) == 0) 
    {        
        if (strlen(cmd) > 5) shell_map_test((char *)cmd+3);   
    }         
    else if (strncmp(cmd, "rm ", 3) == 0) 
    {
        if (strlen(cmd) > 4) remove((char *)(cmd+3));  
    } 
    else if (strncmp(cmd, "mv ", 3) == 0) 
    {        
        if (strlen(cmd) > 5) shell_move((char *)cmd+3);   
    }     
    else if (strncmp(cmd, "cp ", 3) == 0) 
    {        
        if (strlen(cmd) > 5) shell_copy((char *)cmd+3);   
    }          
    else if (strncmp(cmd, "uptime", 4) == 0) 
    {                
        get_delta_string_from_delta_seconds(reply, ITEM_BUF_LEN, unix_time - web.boot_time);
        pico_send_async_text(reply);
    } 
    else if (strncmp(cmd, "ip", 4) == 0) 
    {                
        snprintf(reply, ITEM_BUF_LEN, "ip address: %s\nnet mask:  %s\ngateway:   %s\n", web.ip_address_string, web.network_mask_string, web.gateway_string);
        pico_send_async_text(reply);   
    } 
    else if (strncmp(cmd, "date", 4) == 0) 
    {                
        get_local_date_string(reply, ITEM_BUF_LEN);
        STRNCAT(reply, " ", ITEM_BUF_LEN);
        get_local_time_string(reply + strlen(reply), ITEM_BUF_LEN - strlen(reply)); 
        STRNCAT(reply, "\n", ITEM_BUF_LEN);    
        pico_send_async_text(reply);              
    } 
    else 
    {
        // pass to BASIC interpreter
        hc_load_basic_program((char *)cmd, strlen(cmd));
        hc_queue_send(HC_CMD_BASIC_INTERACTIVE);        
    }
}

/*!
 * \brief Custom File System Override Hooks
 *
 * \param[in]  file  file handle
 * \param[in]  name  file name
 * \return 1 if processed, 0 if not
 */
int fs_open_custom(struct fs_file *file, const char *name) 
{
    static int filesystem_change_counter = 0;
    FILE *automation_file;
    int automation_number = 0;
    char automation_filename[16];
    size_t automaton_file_size = 0;

    if (strcmp(name, "/listen.shtml") == 0) 
    {
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        //file->pextension = NULL;

        // Cache file pointer structure ahead of loop test parsing
        pending_listen_file = file;

        // If data is already sitting in the queue ring, compile and flush instantly
        if (!queue_is_empty()) 
        {
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

    if (strcmp(name, "/post_ok.txt") == 0) 
    {
        static const char *ok_resp = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
        file->data = (char *)ok_resp;
        file->len = strlen(ok_resp);
        file->index = strlen(ok_resp);
        file->flags = FS_FILE_FLAGS_HEADER_INCLUDED;
        //file->pextension = NULL;
        return 1;
    }

    if (strcmp(name, "/post_ok.json") == 0) 
    {
        memset(file, 0, sizeof(struct fs_file));
        file->data = http_200_ok_json;
        file->len  = sizeof(http_200_ok_json) - 1;
        file->index = sizeof(http_200_ok_json) - 1;

        // Tell lwIP's HTTPD state machine that headers are accounted for
        // (This stops the "HTTP headers not included in file system" crash)
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;
        return 1;
    }
    
    if (strcmp(name, "/post_fail.json") == 0) 
    {
        memset(file, 0, sizeof(struct fs_file));
        file->data = http_400_bad_json;
        file->len  = sizeof(http_400_bad_json) - 1;
        file->index = sizeof(http_400_bad_json) - 1;

        // Tell lwIP's HTTPD state machine that headers are accounted for
        // (This stops the "HTTP headers not included in file system" crash)
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;
        return 1;
    }

    if (strcmp(name, "/get_text") == 0) 
    {
        size_t len = strlen(ascii_ram_buffer);
        file->data = ascii_ram_buffer;  // TODO: No idea how this is working, as we are not leaving room for the header, yet below setting the flag for header included
        file->len = len;
        file->index = len;
        //file->pextension = NULL;
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;

        // Cache file pointer structure ahead of loop test parsing
        pending_get_text = file;        
        return 1;
    }

    if (strncmp(name, "/get_automation", 15) == 0) 
    {
        sscanf(name+15, "%d", &automation_number);
        sprintf(automation_filename, "automation%02d", automation_number);

        automation_file = fopen(automation_filename, "rb");
        if (automation_file)
        {
            automaton_file_size = fread(ascii_ram_buffer, 1, MAX_PROGRAM_SIZE, automation_file);   // TODO: No idea how this is working, as we are not leaving room for the header, yet below setting the flag for header included
            ascii_ram_buffer[automaton_file_size] = 0;
            fclose(automation_file);
        }
        else
        {
            printf("failed to open file %s\n", automation_filename);
            ascii_ram_buffer[0] = 0;
        }

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
    
// THIS WORKS BUT does not dynamically update the dictionary when new files are added    
//  if (strcmp(name, "/commands.json") == 0) 
//     {
//         ascii_ram_buffer[0] = 0;
//         picofs_generate_tab_completion_file_list(ascii_ram_buffer, sizeof(ascii_ram_buffer));

//         size_t len = strlen(ascii_ram_buffer);
//         file->data = ascii_ram_buffer;
//         file->len = len;
//         file->index = len;
//         //file->pextension = NULL;
//         file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;

//         // Cache file pointer structure ahead of loop test parsing
//         pending_get_text = file;        
//         return 1;
//     }    

// Inside your file routing function...
if (strncmp(name, "/commands.json", 14) == 0) 
{
    tab_completion_buffer[0] = 0;
    static int system_file_version = 1; 

    // Construct the string we are searching for (e.g., "?v=1")
    char version_query[32];
    snprintf(version_query, sizeof(version_query), "?v=%d", system_file_version);

    // If the browser passed our exact current version value in the URL
    if (strstr(name, version_query) != NULL) 
    {
        // Return a zero-body HTTP 304 Not Modified string response
        size_t len = snprintf(tab_completion_buffer, sizeof(tab_completion_buffer),
            "HTTP/1.1 304 Not Modified\r\n"
            "Connection: keep-alive\r\n"
            "\r\n");

        file->data = tab_completion_buffer;
        file->len = len;
        file->index = len;
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;
        
        pending_get_text = file;        
        return 1;
    }
    else 
    {
        // 1. Build a strict layout header template block. 
        // We hardcode a known 5-digit placeholder string ("00000") for Content-Length.
        int header_len = snprintf(tab_completion_buffer, sizeof(tab_completion_buffer),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 00000\r\n" 
            "\r\n");

        // 2. Generate JSON directly into ascii_ram_buffer immediately following the header block
        char *json_start_ptr = tab_completion_buffer + header_len;
        size_t available_json_space = sizeof(tab_completion_buffer) - header_len;
        
        picofs_generate_tab_completion_file_list(json_start_ptr, available_json_space);
        size_t json_len = strlen(json_start_ptr);

        // 3. Find the exact text window location of our "00000" placeholder sequence inside the buffer
        // and patch it manually using snprintf safely inside its isolated slot.
        char *length_placeholder_ptr = strstr(tab_completion_buffer, "Content-Length: ");
        if (length_placeholder_ptr != NULL) {
            // Step forward 16 characters past "Content-Length: " to target "00000"
            char *target_digits = length_placeholder_ptr + 16;
            
            // Print exactly 5 digits into this window slot.
            // This overwrites "00000" with your actual size (e.g., "00142") 
            // without adding an accidental null-terminator byte to cut off your JSON string.
            char temp_digits[6];
            snprintf(temp_digits, sizeof(temp_digits), "%05d", (int)json_len);
            memcpy(target_digits, temp_digits, 5);
        }

        // 4. Calculate total explicit layout length safely
        size_t total_len = header_len + json_len;
        file->data = tab_completion_buffer;
        file->len = total_len;
        file->index = total_len;
        file->flags |= FS_FILE_FLAGS_HEADER_INCLUDED;

        pending_get_text = file;        
        return 1;
    }
    
}    
    return 0;
}

// /**
//  * lwIP Hook: Optional callback triggered if LWIP_HTTPD_SUPPORT_CUSTOM_HEADERS is enabled.
//  * It catches the incoming If-None-Match header from the browser before fs_open is processed.
//  */
// err_t httpd_parse_custom_header(void *connection_id, const char *header_name, const char *header_value)
// {
//     if (strcasecmp(header_name, "If-None-Match") == 0) {
//         // Strip quotes if the browser passed them (e.g., "v1" -> v1)
//         strncpy(last_client_etag, header_value, sizeof(last_client_etag) - 1);
//     }
//     return ERR_OK;
// }

/*!
 * \brief close custom file
 *
 * \param[in]  file  file handle
 * \return 1 if processed, 0 if not
 */
void fs_close_custom(struct fs_file *file) 
{
    if (pending_listen_file == file) 
    {
        pending_listen_file = NULL;
    }
    // *******THIS CODE IS UNRELATED TO THE SHELL -- it is for the CGI_SSI web_context memory to be released
    // else if (file != NULL && file->state != NULL)
    // {
    //     printf("RELEASING web context memory @%p\n", file->state);
    //     mem_free(file->state);
    //     file->state = NULL;


    //     hex_dump(file->data, 32);
    // }

    // TEST TEST TEST         AUTORUN BASIC SCRIPT AFTER FILE UPLOAD
    // if (pending_get_text == file) 
    // {
    //     pending_get_text = NULL;
    //     hc_queue_send(HC_CMD_BASIC_SCRIPT);        
    // }    
}



/**
 * Standard HTTP POST Handlers for /exec commands
 */


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
            if (current_buffer_index + q->len < (MAX_PROGRAM_SIZE - 1)) {
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
            //hc_queue_send(HC_CMD_BASIC_SCRIPT);    // run basic script after file downloaded
            
            sprintf(web.edit_text_filename, "automation%02d", ((WEB_SESSION_STATE_T *)connection)->automation_file_number);
            
            hc_queue_send(HC_CMD_SAVE_TEXT_FILE);    // save text file adfter file downloaded
        } else {
            snprintf(response_uri, response_uri_len, "/post_fail.json");
            printf("Rejected response\n");
        }    
        break;
    }
}

void init_shell_backend(void) {
    // Hooks initialized implicitly via file architecture layer structures
}


/*!
 * \brief Blocking method to print a string to browser FOR USE FROM APPLICATION TASKS!  DO NOT USE WITHIN THIS FILE
 *
 * \param[in]  text  text to print
 * \return nothing
 */
void shell_print_string(const char* text) 
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

/*!
 * \brief Blocking method to printf to browser FOR USE FROM APPLICATION TASKS!  DO NOT USE WITHIN THIS FILE
 *
 * \param[in]  format  printf format specifier
 * \param[in]  vargs   list of arguments
 * \return nothing
 */
void shell_printf(const char *format, ...)
{
    va_list args;
    int retry = 0;
    bool complete = false;

    if (format == NULL) return;
    
    do
    {
        // Guard against potential thread concurrency collisions
        cyw43_arch_lwip_begin();

        if (!queue_is_full()) 
        {
            // Copy string payload safely into current head index
            memset(queue_storage[queue_head], 0, ITEM_BUF_LEN);
            //vsnprintf(queue_storage[queue_head], ITEM_BUF_LEN, "%s", text);

            va_start(args, format);
            vsnprintf(queue_storage[queue_head], ITEM_BUF_LEN, format, args);
            va_end(args);  

            queue_lengths[queue_head] = strlen(queue_storage[queue_head]);

            // Shift head circular tracking index ahead
            queue_head = (queue_head + 1) % QUEUE_DEPTH;

            // Instantly verify if a browser connection is waiting to take this item
            check_and_flush_queue();
            complete = true;
            cyw43_arch_lwip_end();            
        } 
        else
        {
            // Optional: Log an internal overflow drop event error here if buffer size isn't deep enough
            cyw43_arch_lwip_end();
            SLEEP_MS(10);
        }

        //cyw43_arch_lwip_end(); DANGER! Make sure we always release the lock!!!

        retry++;
    } while (!complete && retry < 200);

    //if (retry >= 200) printf("DROP\n");

}

/*!
 * \brief print the contents of the ascii buffer / BASIC program
 *
 * \return nothing
 */
void dump_text_buffer(void)
{
    int i;

    printf("PROGRAM TEXT\n");
    for(i=0; i < MAX_PROGRAM_SIZE && basic_program[i] != 0; i++)
    {
        //if(!(i%80)) printf("\n");

        printf("%c", basic_program[i]);        
    }
    printf("\n");
    
    printf("PROGRAM HEX DUMP\n");
    for(i=0; i < MAX_PROGRAM_SIZE && basic_program[i] != 0; i++)
    {
        if(!(i%80)) printf("\n");

        printf("%02x ", basic_program[i]);        
    }    
    printf("\n");    
}

/*!
 * \brief edit the content of a text file
 *
 * \param[in]  filename  file to print to shell
 * \return nothing
 */
int shell_edit(char *filename)
{
    FILE *filePointer = NULL;
    int file_size = 0;
    char buffer[256];


    // initialize ascii buffer
    memset(ASCII_HEADER_SIZE + ascii_ram_buffer, 0, sizeof(ascii_ram_buffer) - ASCII_HEADER_SIZE);
    strcpy(ascii_ram_buffer, http_ascii_buffer_header_tmpl);  // prepend http header to ascii ram buffer
    
    // remember name of file under edit
    STRNCPY(web.edit_text_filename, filename, sizeof(web.edit_text_filename));

    // empty the ascii buffer data section
    ascii_ram_buffer[ASCII_HEADER_SIZE] = 0;

    filePointer = fopen(filename, "rb");
    if (filePointer)
    {
        file_size = fread(ascii_ram_buffer + ASCII_HEADER_SIZE, 1, MAX_PROGRAM_SIZE, filePointer);
        ascii_ram_buffer[ASCII_HEADER_SIZE + file_size] = 0;

        printf("file_size = %d\n", file_size);
        printf("file = %s\n", ascii_ram_buffer + ASCII_HEADER_SIZE);        
        fclose(filePointer);
    }
    else
    {
        shell_printf_nb("edit: creating new file: %s\n", filename);
        ascii_ram_buffer[ASCII_HEADER_SIZE] = 0;
    }

    // close the file to free up system resources
    fclose(filePointer);

    return 0;
}

/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  filename  file to print to shell
 * \return nothing
 */
int shell_cat(char *filename)
{
    FILE *filePointer;
    char buffer[128];

    // open the file in read mode ("r")
    filePointer = fopen(filename, "r");

    // check if the file exists and opened successfully
    if (filePointer == NULL) 
    {
        shell_printf_nb("cat: %s: No such file\n", filename);
        return 1; 
    }

    // read and print the file line-by-line
    while (fgets(buffer, sizeof(buffer), filePointer) != NULL)
    {
        shell_printf_nb("%s", buffer);              //TODO: call shell_cat() from another task and use blocking shell_printf to avoid data loss
    }

    // close the file to free up system resources
    fclose(filePointer);

    return 0;
}

/*!
 * \brief print hex dump of buffer
 * 
 * \param[in]  filename  file to dump to shell
 * \return nothing
 */
int shell_hex_dump(char *filename)
{
    FILE *filePointer;
    unsigned char buffer[16];
    size_t bytes_read = 0;
    char output_line[160];
    char output_byte[8];
    int i;

    printf("hexdump %d\n", filename);

    // 1. Open the file in read binary mode ("r")
    filePointer = fopen(filename, "rb");

    // 2. Check if the file exists and opened successfully
    if (filePointer == NULL) 
    {
        shell_printf_nb("hd: %s: No such file\n", filename);
        return 1; 
    }

    // 3. Read and print the file 16 bytes at a time
    while (bytes_read = fread(buffer, 1, sizeof(buffer), filePointer))
    {
        if(bytes_read)
        {
            output_line[0] = 0;

            for (i = 0; i < bytes_read;i++) 
            {
                snprintf(output_byte, sizeof(output_byte), "%02x ", buffer[i]);
                STRAPPEND(output_line, output_byte);
            }            

             // add extra padding if less than 16 bytes (i.e. last line)
            for (; i<16; i++)
            {
                STRAPPEND(output_line, "   ");
            }

            STRAPPEND(output_line, " ");  
            for (i = 0; i < bytes_read; i++) 
            {
                if (isprint(buffer[i]))
                {
                   snprintf(output_byte, sizeof(output_byte), "%c", buffer[i]);
                   STRAPPEND(output_line, output_byte);
                }
                else
                {
                    snprintf(output_byte, sizeof(output_byte), "-");
                    STRAPPEND(output_line, output_byte);
                }
            }

            STRAPPEND(output_line, "\n");
            shell_printf_nb("%s", output_line);                //TODO: call shell_hex_dump() from another task and use blocking shell_printf to avoid data loss
        }        
    }

    // 4. Close the file to free up system resources
    fclose(filePointer);

    return(0);
}

/*!
 * \brief ping - icmp echo request
 * \param[in]  ipv4_string  ip address to send icmp echo 
 * \return 0 on success, -1 on error
 */
int shell_ping(char *ipv4_string)
{
    u32_t ip;
    int values[4] = {0,0,0,0};
    u8_t byte = 0;
    int i;

    // static int number_of_shelly_devices = 0;
    // char device_type[32];
    // char device_id[32];
    ip_addr_t ping_addr;
    int ping_err = -1;

    sscanf(ipv4_string, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);
    
    ip   = 0x00000000;
    for(i=0; i<4; i++)
    {
        byte = (u8_t)values[i];
        ip = ip<<8 | byte;
    }
    ping_addr.addr = htonl(ip);

    shell_printf_nb("PING %d.%d.%d.%d with 32 bytes of data\n", ((u8_t *)&ip)[3], ((u8_t *)&ip)[2], ((u8_t *)&ip)[1], ((u8_t *)&ip)[0]);

    ping_err = ping_device(&ping_addr, 3);

    if (!ping_err)
    {
        shell_printf_nb("ping successful\n");
    } 
    else
    {
        shell_printf_nb("ping failed with error %d\n", ping_err);
    }

    return(0);
}

/*!
 * \brief Non-Blocking method to printf to browser --USE WITHIN THIS FILE / overflow will be discarded--
 *
 * \param[in]  format  printf format specifier
 * \param[in]  vargs   list of arguments
 * \return nothing
 */
void shell_printf_nb(const char *format, ...)
{
    va_list args;
    int retry = 0;
    bool complete = false;


    // Guard against potential thread concurrency collisions
    cyw43_arch_lwip_begin();

    if (!queue_is_full()) 
    {
        // Copy string payload safely into current head index
        memset(queue_storage[queue_head], 0, ITEM_BUF_LEN);
        //vsnprintf(queue_storage[queue_head], ITEM_BUF_LEN, "%s", text);

        va_start(args, format);
        vsnprintf(queue_storage[queue_head], ITEM_BUF_LEN, format, args);
        va_end(args);  

        queue_lengths[queue_head] = strlen(queue_storage[queue_head]);

        // Shift head circular tracking index ahead
        queue_head = (queue_head + 1) % QUEUE_DEPTH;

        // Instantly verify if a browser connection is waiting to take this item
        check_and_flush_queue();
        complete = true;
        
    } else
    {
        // Optional: Log an internal overflow drop event error here if buffer size isn't deep enough
    }

    cyw43_arch_lwip_end();

}

/*!
 * \brief move a file (rename)
 *
 * \param[in]  old_name  file source
 * \param[in]  new_name  file destination
 * \return nothing
 */
int shell_move(char *src_space_dst)
{
    FILE *filePointer;
    char buffer[256];
    int i, j;
    char src[16];
    char dst[16];

    src[0] = 0;
    dst[0] = 0;

    // extract source
    for(i=0; i<strlen(src_space_dst); i++)
    {
        if (src_space_dst[i] == ' ')
        {
            src[i++] = 0;
            break;
        }

        src[i] = src_space_dst[i];
        src[i+1] = 0;
    }

    // extract destination
    j = 0;
    for(; i<strlen(src_space_dst); i++)
    {
        if ((src_space_dst[i] == 0) || (!isprint(src_space_dst[i])))
        {
            dst[j] = 0;
            break;
        }

        dst[j++] = src_space_dst[i];
        dst[j] = 0;
    }

    if ((strlen(src) > 2) && (strlen(dst) > 2))
    {
        rename(src, dst);
    }

    return 0;
}

/*!
 * \brief copy a file
 *
 * \param[in]  old_name  file source
 * \param[in]  new_name  file destination
 * \return nothing
 */
int shell_copy(char *src_space_dst)
{
    FILE *filePointer;
    char buffer[256];
    int i, j;
    char src[16];
    char dst[16];

    src[0] = 0;
    dst[0] = 0;

    // extract source
    for(i=0; i<strlen(src_space_dst); i++)
    {
        if (src_space_dst[i] == ' ')
        {
            src[i++] = 0;
            break;
        }

        src[i] = src_space_dst[i];
        src[i+1] = 0;
    }

    // extract destination
    j = 0;
    for(; i<strlen(src_space_dst); i++)
    {
        if ((src_space_dst[i] == 0) || (!isprint(src_space_dst[i])))
        {
            dst[j] = 0;
            break;
        }

        dst[j++] = src_space_dst[i];
        dst[j] = 0;
    }

    if ((strlen(src) > 2) && (strlen(dst) > 2))
    {
        picofs_copy(src, dst);
    }

    return 0;
}



/*!
 * \brief print hex dump of buffer
 * 
 * \param[in]  filename  file to dump to shell
 * \return nothing
 */
int shell_map_test(char *filename)
{
    FILE *filePointer;
    unsigned char buffer[16];
    size_t bytes_read = 0;
    char output_line[160];
    char output_byte[8];
    int i;
    int fd;
    void *file_data = NULL;

    printf("mmap test %d\n", filename);

    // 1. Open the file in read binary mode ("r")
    filePointer = fopen(filename, "rb");

    // 2. Check if the file exists and opened successfully
    if (filePointer == NULL) 
    {
        shell_printf_nb("hd: %s: No such file\n", filename);
        return 1; 
    }

    fd = fileno(filePointer);
    
    
    file_data = picofs_mmap(NULL, 0, PROT_READ, MAP_SHARED, fd, 0);

    shell_printf_nb("mt: mapping via fopen(): %p\n", file_data);
    shell_printf_nb("%s\n", file_data);

    // 4. Close the file to free up system resources
    fclose(filePointer);


    fd = open(filename, O_RDONLY);
    file_data = picofs_mmap(NULL, 0, PROT_READ, MAP_SHARED, fd, 0);

    shell_printf_nb("mt: mapping via open(): %p\n", file_data);
    shell_printf_nb("%s\n", file_data);
    
    close(fd);

    return(0);
}

