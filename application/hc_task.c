
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

// TODO - prune this list of includes
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
        config.personality = HOME_CONTROLLER;
    }    
    
    printf("home controller task initializing file system...\n");
    picofs_initialize();

    printf("home controller task started\n");
    while (true)
    {
        // initialize all subsystems that are not already up
        hc_initialize();

        //basic_Interpreter(NULL, NULL, my_program, sizeof(my_program));

        //dump_text_buffer();

        if ((config.personality == HOME_CONTROLLER))
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
                    basic_Interpreter(NULL, NULL, basic_program_buffer, strlen(basic_program_buffer), false);
                    break;
                case HC_CMD_BASIC_SCRIPT:
                    basic_program[current_buffer_index+1] = 0;
                    basic_Interpreter(NULL, NULL, basic_program, current_buffer_index, true);                
                    break;
                case HC_CMD_BASIC_FILE:
                    // basic_program[current_buffer_index+1] = 0;
                    // basic_Interpreter(NULL, NULL, basic_program, current_buffer_index, true);                  
                    // printf("DELAY 5 SECONDS\n");
                    // SLEEP_MS(5000);
                    // printf("Will now attempt to run script %s\n", web.basic_file_to_execute);
                    basic_Interpreter(web.basic_file_to_execute, NULL, NULL, 0, true);                
                    break; 
                case HC_CMD_CAT_FILE:
                    hc_cat(web.file_to_cat);                
                    break;                     
                                      
                case HC_CMD_LIGHTS:
                    shelly_http_request(HTTP_GET, "/relay/1?turn=on", "192.168.33.165", NULL);
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
                    picofs_erase_obsolete_sectors();
                    break;                
                case HC_CMD_DEFRAGMENT:
                    //picofs_consolidate_all_files();
                    picofs_consolidate_all_files_in_flash();
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
   
    //printf("Saving file: %s\n", web.edit_text_filename);

    file_ptr = fopen(web.edit_text_filename, "w");

    if (file_ptr == NULL) 
    {
        printf("ERROR opening file\n");
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    if (fputs(editor_text, file_ptr) == EOF) 
    {
        printf("ERRO writing file\n");
        perror("Error writing to file");
        fclose(file_ptr);
        return EXIT_FAILURE;
    }

        fclose(file_ptr);

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