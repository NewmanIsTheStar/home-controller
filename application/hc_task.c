
/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>


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
#include "queue.h"

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
#include "hc_task.h"
#include "basic.h"
//#include "http_post.h"
#include "shell.h"
#include "picofs.h"
#include "ping_core.h"


//#define DEBUG_UDP_MESSAGES
#define HC_TASK_LOOP_DELAY (60*1000)

//#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
// typdedefs
typedef struct
{
    int (*initialization)(void);
    bool initialization_complete;
} HC_INITIALIZATION_T;




//prototypes
int hc_wait(TickType_t timeout);
int hc_initialize_queue(void);
int hc_initialize(void);
int hc_save_text_file_from_ascii_buffer(void);
int hc_cat(char *filename);
int hc_hex_dump(char *filename);
void copy_first_line(char *dest_buffer, const char *source_buffer, size_t dest_size);
int hc_delete_file(void);

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern char *basic_program;
extern size_t current_buffer_index;
extern char ascii_ram_buffer[];
extern const char *editor_text;

//static variables
void *watchdog_params = NULL;
char my_program[] = "FOR x = 1 TO 100\nPRINT \"HELLO WORLD! \";X\nNEXT";
//char my_program[] = "5 X = 1\n10 PRINT \"HELLO WORLD! \" + X\n15 X = X +1\n20 GOTO 10";
//char my_program[] = "5 X = 1\n15 X = X +1\n20 GOTO 15";
//char my_program[] = "5 X = 1\n10 PRINT \"HELLO WORLD!\" + X\n12 SLEEP 1\n15 X = X +1\n20 GOTO 10";
static HC_INITIALIZATION_T hc_initialization_table[] =
{

    {hc_initialize_queue,                     false},                    
};
static QueueHandle_t hc_queue = NULL;                     
static uint8_t hc_message = 0;                            
static bool hc_queue_initialized = false;
char basic_program_buffer[4096];    

/*!
 * \brief home controller task
 *
 * \param[in]  params  alive counter that must be incremented periodically to prevent watchdog reset
 * 
 * \return nothing
 */
void hc_task(__unused void *params) 
{
    SOCKADDR_IN sClientAddress;  
    int received_bytes = 0; 
    int hc_request = 0;  
    int i; 
    
    // store passed watchdog parameter 
    watchdog_params = params;

    if (strcasecmp(APP_NAME, "home-controller") == 0)
    {
        // force personality to match single purpose application
        cfg->personality = HOME_CONTROLLER;
    }    
    
    // printf("home controller task initializing file system...\n");
    // picofs_initialize();

    printf("home controller task started\n");
    while (true)
    {
        // initialize all subsystems that are not already up
        hc_initialize();

        //basic_Interpreter(NULL, NULL, my_program, sizeof(my_program));

        //dump_text_buffer();

        if ((cfg->personality == HOME_CONTROLLER))
        {
            //TEST TEST TEST
            // printf("Begin shelly test\n");
            // discover_shelly_devices();
            // printf("End shelly test\n");
            printf("Home Controller\n");
            //pico_send_async_text("This is async text from hc_task");

            // TEST TEST TEST
            // SLEEP_MS(30000);
            // printf("Begin Test of shell buffer satuartion\n");
            // for(i=0; i<100; i++)
            // {
            //     // snprintf(reply, sizeof(reply), "pong line a quick brown fox jumps over the lazy dog %012d!\n", i);
            //     // pico_send_async_text(reply);         
            //     shell_printf("pong line a quick brown fox jumps over the lazy dog %012d!\n", i); 
            // }
            // printf("End Test of shell buffer satuartion\n");

            // wait for timeout period but abort immediately if a command is received
            hc_request = hc_wait(HC_TASK_LOOP_DELAY);

            if (hc_request)
            {
                switch(hc_message)
                {
                case HC_CMD_BASIC_INTERACTIVE:
                    basic_Interpreter(IM_INTERACTIVE, NULL, NULL, basic_program_buffer, strlen(basic_program_buffer));
                    break;
                case HC_CMD_BASIC_SCRIPT:
                    basic_program[current_buffer_index++] = 0;
                    basic_Interpreter(IM_EXECUTE_IN_PASSED_RAM_BUFFER, NULL, NULL, basic_program, current_buffer_index);                
                    break;
                case HC_CMD_BASIC_FILE:
                    basic_Interpreter(IM_MMAP_FILE, NULL, web.basic_file_to_execute, NULL, 0);                
                    break; 
                case HC_CMD_CAT_FILE:
                    hc_cat(web.file_to_cat);                
                    break;      
                case HC_CMD_HEXDUMP_FILE:
                    hc_hex_dump(web.file_to_hexdump);                
                    break;                                                                        
                case HC_CMD_LIGHTS:
                    //shelly_http_request(HTTP_GET, "/relay/1?turn=on", "192.168.33.165", NULL);
                    //config_mmap_test();
                    //config_write_to_file("config.bin"); 
                    config_mmap("config.bin");
                    strcpy(cfg->automation_name[31], "A31 set via mmap");
                    picofs_msync(cfg, sizeof(NON_VOL_VARIABLES_T), MS_SYNC);
                    break; 
                case HC_CMD_SHELLY_DEVICE_DUMP :
                    shelly_cache_device_dump(web.shelly_device_ip);
                    break;
                case HC_CMD_DUMP_PROGRAM:
                    dump_text_buffer();
                    break;   
                case HC_CMD_LIST:
                    picofs_list_all_files_from_cache();  // valid files in cache
                    break;
                case HC_CMD_LIST_CORRUPT:
                    picofs_list_all_files_from_flash(true);  // all files in flash including corrupted ones
                    break;                    
                case HC_CMD_PING:
                    shell_ping(web.ping_target);
                    break;
                case HC_CMD_PAGE_MAP:    
                    picofs_find_page_status(PFS_DISPLAY_SHELL_PAGE_MAP);
                    break;
                case HC_CMD_PAGE_NUMBERS:
                    picofs_find_page_status(PFS_DISPLAY_SHELL_PAGE_NUMBERS);
                    break;
                case HC_CMD_SAVE_TEXT_FILE:
                    hc_save_text_file_from_ascii_buffer();
                    break;  
                case HC_CMD_DISK_CLEANUP:
                    picofs_erase_obsolete_sectors(false);
                    break;                
                case HC_CMD_DEFRAGMENT:
                    //picofs_consolidate_all_files();
                    picofs_consolidate_all_files_in_flash();
                    break;
                case HC_CMD_DELETE_FILE:                    
                    hc_delete_file();
                    break;
                default:
                    printf("HC task received unrecognized message (%d)\n", hc_message);
                    break;
                }                
            }
        }
        else
        {
            SLEEP_MS(1000);
        }

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params);  
    } 
}

/*!
 * \brief pat the watchdog
 * 
 * \return nothing
 */
void hc_pat_watchdog(void) 
{
    // this function exists so that during the execution of basic scripts the watchdog may be updated
    watchdog_pulse((int *)watchdog_params); 
}

/*!
 * \brief wait for timeout or queue
 * 
 * \return true if timeout preempted
 */
int hc_wait(TickType_t timeout)
{
    int err = 0;

    hc_message = HC_CMD_UNKNOWN;

    if (xQueueReceive(hc_queue, &hc_message, timeout) == pdPASS)
    {
        // got a message
        err = 1;
    }

    return(err);
}

/*!
 * \brief send a message to the mqtt_task queue
 *
 * \param message one byte message
 * 
 * \return nothing
 */
void hc_queue_send(uint8_t message)
{
    static uint8_t message_store = 0;

    if (hc_queue_initialized)
    {
        message_store = message;

        // send the message to the queue
        xQueueSend(hc_queue, &message_store, 0);
    }
}

/*!
 * \brief initialize a queue for sending messages to the mqtt_task
 * 
 * \return nothing
 */
int hc_initialize_queue(void)
{
    int err = 0;

    // create queue for to pass interrupt messages to task
    hc_queue = xQueueCreate(1, sizeof(uint8_t));

    hc_queue_initialized = true;

    return(err);
}

/*!
 * \brief initialize subsystems
 *
 * \param params none
 * 
 * \return 0 on success
 */
int hc_initialize(void)
{
    static bool init_complete = false;
    static int  attempt = 0;
    int err = 0;
    int i;

    for (i=0; i < NUM_ROWS(hc_initialization_table); i++)
    {
        if (!hc_initialization_table[i].initialization_complete)
        {
            hc_initialization_table[i].initialization_complete = !hc_initialization_table[i].initialization();            

            if (!hc_initialization_table[i].initialization_complete)
            {
                err++;
                printf("HC incomplete initialization of subsystem %d at attempt %d\n", i, attempt);
                init_complete = false;
            }
        }
    }

    if (err)
    {
        printf("HC %d subsystem%s failed to initialize during attempt %d\n", err, err>1?"s":"", attempt);
        
    } else if (!init_complete)
    {
        printf("HC all subsystems sucessfully initialized at attempt %d\n", attempt);
        init_complete = true;
        attempt = 0;
    }

    return(err);
}


/*!
 * \brief send a message to the mqtt_task queue
 *
 * \param message one byte message
 * 
 * \return nothing
 */
void hc_load_basic_program(char *program, int len)
{

    //memcpy(basic_program_buffer, program, len);
    strcpy(basic_program_buffer, program);
    //strcat(basic_program_buffer, "\r\nEND\r\n");  // not needed because interpreter does this when loading from ram
}

/*!
 * \brief send a message to the mqtt_task queue
 *
 * \param message one byte message
 * 
 * \return nothing
 */
int hc_save_text_file_from_ascii_buffer(void)
{
    FILE *file_ptr = NULL;
    char filename[16];
    int automation_number = 0;
    char *automation_name;
    char *body;
   
    // extract filename from first line of the buffer
    copy_first_line(filename, editor_text, sizeof(filename)); 

    if (filename[0] == 0)
    {
        strncpy(filename, "unnamed", sizeof(filename));
    }

    // special case: automations contain the user defined descriptive name on the second buffer line
    if (strncmp(filename, "automation", 10) == 0)
    {
        sscanf(filename+10, "%d", &automation_number);
        CLIP(automation_number, 0, 31);
        automation_name = strchr(editor_text, '\n') + 1;

        copy_first_line(cfg->automation_name[automation_number], automation_name, sizeof(cfg->automation_name[automation_number])); 

        body = strchr(automation_name, '\n') + 1;  
        
        cfg->automation_status[automation_number] = AUTOMATION_ENABLED;
        printf("setting status for automation number %d to Enabled\n", automation_number);        
    }
    else
    {
        body = strchr(editor_text, '\n') + 1;
    }

    if ((body - editor_text) < MAX_PROGRAM_SIZE)
    {
        file_ptr = fopen(filename, "w");

        if (file_ptr == NULL) 
        {
            printf("ERROR opening file\n");
            perror("Error opening file");
            return EXIT_FAILURE;
        }

        if (fputs(body, file_ptr) == EOF) 
        {
            printf("ERRO writing file\n");
            perror("Error writing to file");
            fclose(file_ptr);
            return EXIT_FAILURE;
        }

        fclose(file_ptr);


    }
    //printf("Data successfully saved to output.txt\n");

    return EXIT_SUCCESS;
}


/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  filename  file to print to shell
 * \return nothing
 */
int hc_cat(char *filename)
{
    FILE *filePointer;
    char buffer[128];

    // open the file in read mode ("r")
    filePointer = fopen(filename, "r");

    // check if the file exists and opened successfully
    if (filePointer == NULL) 
    {
        shell_printf("cat: %s: No such file\n", filename);
        return 1; 
    }

    // read and print the file line-by-line
    while (fgets(buffer, sizeof(buffer), filePointer) != NULL)
    {
        shell_printf("%s", buffer); 
        // printf("%s", buffer);
        // SLEEP_MS(100);
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
int hc_hex_dump(char *filename)
{
    FILE *filePointer;
    unsigned char buffer[16];
    size_t bytes_read = 0;
    char output_line[160];
    char output_byte[8];
    int i;

    printf("hexdump %s\n", filename);

    // 1. Open the file in read binary mode ("r")
    filePointer = fopen(filename, "rb");

    // 2. Check if the file exists and opened successfully
    if (filePointer == NULL) 
    {
        shell_printf("hd: %s: No such file\n", filename);
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
            shell_printf("%s", output_line);                //TODO: call shell_hex_dump() from another task and use blocking shell_printf to avoid data loss
        }        
    }

    // 4. Close the file to free up system resources
    fclose(filePointer);

    return(0);
}

void copy_first_line(char *dest_buffer, const char *source_buffer, size_t dest_size) 
{
    // find the first occurrence of the newline character
    const char *newline = strchr(source_buffer, '\n');
    size_t line_length;

    if (newline != NULL) 
    {
        // calculate the length up to (but excluding) the newline
        line_length = newline - source_buffer;
    } else 
    {
        // if there is no newline, the first line is the entire string
        line_length = strlen(source_buffer);
    }

    // prevent buffer overflow by fitting inside the destination
    if (line_length >= dest_size) 
    {
        line_length = dest_size - 1; 
    }

    // copy the line and manually null-terminate it
    strncpy(dest_buffer, source_buffer, line_length);
    dest_buffer[line_length] = '\0';
}

int hc_get_new_automation_number(void)
{
    int err = -1;
    int i;

    for(i=0; i < NUM_ROWS(cfg->automation_status); i++)
    {
        if (cfg->automation_status[i] == AUTOMATION_UNDEFINED)
        {
            cfg->automation_status[i] = AUTOMATION_DISABLED;
            err = 0;
            break;
        }
    }

    if (!err)
    {
        err = i;
    }

    return(err);    
}

int hc_delete_file(void)
{
    int err = -1;

    if (web.delete_filename[0])
    {
        err = remove(web.delete_filename);
    }

    return(err);
}