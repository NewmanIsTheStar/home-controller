/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lwip/opt.h"
#if !LWIP_HTTPD_SUPPORT_WEBSOCKET
#error "LWIP_HTTPD_SUPPORT_WEBSOCKET must be enabled in lwipopts.h to use this file!"
#endif

#include <sys/stat.h> 
#include "pico/cyw43_arch.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"
#include "hardware/structs/powman.h"
#include "hardware/regs/powman.h"
#include <hardware/flash.h>
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cgi.h"
#include "utility.h"
#include "config.h"
#include "application_config.h"
#include "watchdog.h"
#include "worker_tasks.h"
#include "pluto_wifi_country.h"
#include "pluto.h"
#include "picofs.h"
#include "pluto_swload.h"
#include "calendar.h"
#include "system_web.h"
#include "ssi.h"

#ifdef USE_GIT_HASH_AS_VERSION
#include "githash.h"
#endif

#ifndef APP_NAME
#define APP_NAME "PLUTO"
#endif

#ifndef RUN_FREERTOS_ON_CORE
#define RUN_FREERTOS_ON_CORE 0
#endif

#define BOSS_TASK_PRIORITY              ( 0 + 1UL )
#define WATCHDOG_TASK_PRIORITY          ( 0 + 1UL )
#define PAUSE_FOR_INITIAL_SNTP_RESPONSE (1)
//#define LOG_CLOCK_SLEW                  (1) 

// uninitialized variable
REBOOT_REASON_T __uninitialized_ram(reboot_reason);

// external variables
extern uint32_t unix_time;
extern APP_CONFIG_T config;
extern SYS_WEB_VARIABLES_T sys_web;
extern WORKER_TASK_T worker_tasks[];

// // static variables
bool restart_requested = false;
// static uint32_t reset_reason;

// long long atomic_variable = 0;

// prototypes
// int pluto(void);
// void boss_task(__unused void *params);
// int pluto_ap_mode(void);
// int set_web_ip_network_info(void);
// int set_realtime_clock(void);
// int monitor_stacks(void);
// int monitor_heap(void);
// int pluto_ap_mode_for_developers(void);
// void atomic_write_task(__unused void *params);
// void atomic_read_task(__unused void *params);
// int set_gpio_defaults(void);
// void print_reset_reason(void);
// void print_tasks_list(void);
// int test_myfilesystem(void);
// int test_stat(void);
// void pluto_get_program_size(void);
// int pluto_download_software(void);
// int pluto_print_startup_status(void);
// int pluto_wifi_connect(void);




/*!
 * \brief Start watchdog, initialize wifi, set clock, start worker tasks then monitor stacks and flash led forever
 *
 * \param params unused garbage
 *
 * \return nothing
 */
void boss_task(__unused void *params) 
{
    bool led_on = false;
    int worker = 0;
    BaseType_t task_creation_status = 0;
    uint32_t clock_ahead_error = 0;
    u8_t *free_flash = NULL;

    
    // start watchdog
    xTaskCreate(watchdog_task, "Watchdog Task", configMINIMAL_STACK_SIZE, NULL, WATCHDOG_TASK_PRIORITY, NULL);

    // initialize file system
    picofs_initialize();

    printf("Searching for configuration...\n");
    
    // get configuration from file system
    config_read(); 

    // default gpio settings 
    set_gpio_defaults();
    
    // initialise wifi
    while (cyw43_arch_init_with_country(get_wifi_country_code(sys->wifi_country)))
    {
         printf("***Failed to initialise wifi***\n");
         cyw43_arch_deinit();
         SLEEP_MS(1000);       
    } 

    // connect to wifi
    pluto_wifi_connect();
 
    // initialize web interface variables
    system_web_init();

    // copy wifi ip info into the web interface variables
    set_web_ip_network_info();

    printf("Address = %s\n", sys_web.ip_address_string);  
    printf("Netmask = %s\n", sys_web.network_mask_string);
    printf("Gateway = %s\n", sys_web.gateway_string);

    // initialize the clock 
    set_realtime_clock(); 

    // start web server
    httpd_init();
    ssi_init();
    cgi_init();

    // start worker tasks
    printf("Starting worker tasks\n");       
    for(worker=0; worker_tasks[worker].functionptr != NULL; worker++)
    {
        task_creation_status = xTaskCreate(worker_tasks[worker].functionptr, worker_tasks[worker].name, worker_tasks[worker].stack_size, &(worker_tasks[worker].watchdog_alive_indicator), worker_tasks[worker].priority, &(worker_tasks[worker].task_handle));
        
        if (task_creation_status != pdPASS)
        {
            printf("WORKER TASK %d :: xTaskCreate failed -- return value [%d]\n", worker, task_creation_status);
        }

        SLEEP_MS(1000);
    }    

    print_tasks_list();
    
    // flash the led for attention while doing no actual work (like a boss!)
    while(true) 
    {
        // toggle led
        cyw43_arch_gpio_put(0, led_on);
        led_on = !led_on;

        // copy configuration changes from RAM into flash
        config_write();

        // check stack high water mark for each worker task
        monitor_stacks();    
        
        // check free heap size
        monitor_heap();

        #ifdef FAKE_RTC
        // maintain fake rtc
        clock_ahead_error = rtc_update();
              
        #ifdef LOG_CLOCK_SLEW 
        if (clock_ahead_error)
        {            
            send_syslog_message("clock", "Clock shaving in progress.  Local clock ahead of UTC by %lu ms", clock_ahead_error);            
        }
        #endif
        #endif

        // update daylight savings start and end dates  TODO: make this less frequent only needed twice per year
        set_daylight_saving_dates(); 

        // report watchdog reboot to syslog server
        check_watchdog_reboot();         

        SLEEP_MS(1000);

        // request reboot if no sntp updates received for 24 hours
        if (!sntp_alive())
        {
            printf("no sntp updates for 24 hours -- requesting reboot\n");
            pluto_application_restart(REBOOT_SNTP_FAILURE);
        }

        // perform software downlaod if requested through web ui
        pluto_download_software();
        
        // reboot if requested
        if (restart_requested)
        {
            printf("Shutdown commenced\n");
            restart_requested = false;

            // disable wifi
            cyw43_arch_disable_sta_mode();
            cyw43_arch_deinit();
            
            if (reboot_reason == REBOOT_USER_REQUEST)
            {
                // flush recent config changes to flash prior to reboot with one retry
                //if (config_write(CONFIG_FILE)) config_write(CONFIG_FILE);
                if (config_write()) config_write();
            }

            printf("***REBOOT in 100 ms***\n");
            watchdog_enable(100, 0);

            SLEEP_MS(1000);
        }         
    }
}


/*!
 * \brief Request application restart 
 *
 * \param none
 *
 * \return nothing
 */
int pluto_application_restart(REBOOT_REASON_T reason)
{
    restart_requested = true;
    reboot_reason = reason;

    return(0);
}
 
/*!
 * \brief get reboot reason stored in RAM by the application prior to reboot
 * 
 * \return integer = value x 10
 */
uint32_t pluto_get_reboot_reason(void)
{
    return(reboot_reason);
}


/*!
 * \brief Set the realtime clock using sntp
 *
 * \param none
 *
 * \return 0
 */
int set_realtime_clock(void)
{
    datetime_t date;
    char buffer[256]; 
    int i;   

    // initialize realtime clock
    #ifndef FAKE_RTC
    rtc_init();
    #endif
    setTimeSec(0);  //1970

    // sntp timeservers
    calendar_timeserver_failsafe();
    sntp_setservername(0, sys->time_server[0]); 
    sntp_setservername(1, sys->time_server[1]); 
    sntp_setservername(2, sys->time_server[2]); 
    sntp_setservername(3, sys->time_server[3]); 

    // snpt start
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();

#ifdef PAUSE_FOR_INITIAL_SNTP_RESPONSE
    //  wait up to 10 seconds for first sntp server response 
    for(i=0; i < 2*10; i++)
    {
        SLEEP_MS(500);

        rtc_get_datetime(&date);
        datetime_to_str(buffer, sizeof(buffer), &date);
        printf("\r%s Zulu         ", buffer);

        if (date.year != 1970) break;
    }
    printf("\n");
#endif    

    // determine daylight savings dates for the current year
    set_daylight_saving_dates(); 

    sys_web.boot_time = unix_time;

    return(0);
}


/*!
 * \brief Track the high water mark for each work task stack
 *
 * \param none
 *
 * \return 0
 */
int monitor_stacks(void)
{
    int worker = 0;
    int chars_remaining = 0;
    int chars_written = 0;
    int char_offset = 0;
    int spam_throttle = 0;

    // monitor stack sizes
    chars_remaining = sizeof(sys_web.stack_message);
    chars_written = 0;
    char_offset = 0;

    for(worker=0; worker_tasks[worker].functionptr != NULL; worker++)
    {
        worker_tasks[worker].stack_high_water_mark = uxTaskGetStackHighWaterMark(worker_tasks[worker].task_handle);
        
        // print stack message for web interface
        if (chars_remaining)
        {
            chars_written = snprintf(sys_web.stack_message+char_offset, chars_remaining, "%s=%lu  ", worker_tasks[worker].name, worker_tasks[worker].stack_high_water_mark);
            if ((chars_written >0) && (chars_written < chars_remaining))
            {
                char_offset += chars_written;
                chars_remaining = sizeof(sys_web.stack_message) - char_offset;
            }
        }

        if (worker_tasks[worker].stack_high_water_mark < 50)
        {
            if (!spam_throttle)
            {
                send_syslog_message("stack", "LOW STACK WARNING.  %s stack high water mark = %d", worker_tasks[worker].name, worker_tasks[worker].stack_high_water_mark);
                spam_throttle = 60;
            }
            else
            {
                spam_throttle--;
            }
        }
    } 

    return(0);
}

/*!
 * \brief Track the minimum heap size
 *
 * \param none
 *
 * \return 0
 */
int monitor_heap(void)
{
    static size_t min_heap_free = UINT_MAX;
    size_t current_heap_free = 0;

    current_heap_free = xPortGetMinimumEverFreeHeapSize();

    if (current_heap_free < min_heap_free)
    {
        min_heap_free = current_heap_free;
        printf("Minimum heap = %lu\n", min_heap_free);
    }

    return(0);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) 
{
    // Hardcoded breakpoint instruction tells the SWD debugger to freeze right here
    __asm volatile("bkpt #0"); 

    while(true)
    {
        printf("stack overflow\n");
    }
}


/*!
 * \brief Dump list of running tasks including priorities
 *
 * \return nothing
 */
void print_tasks_list(void) 
{
    // Allocate a buffer large enough to hold the text table 
    // (Roughly 40 bytes per task is safe)
    char buffer[512]; 
    
    printf("Task Name\tState\tPrio\tStack\tNum\n");
    printf("-------------------------------------------\n");
    
    // Generate the list
    vTaskList(buffer);
    
    // Print the formatted string
    printf("%s\n", buffer);
}


/*!
 * \brief Set IP address, netmask and gateway in web interface 
 *
 * \param none
 *
 * \return 0
 */
int set_web_ip_network_info(void)
{
    // copy ip, netmask and gateway assigned by DHCP into web interface variables
    STRNCPY(sys_web.ip_address_string, ipaddr_ntoa(netif_ip4_addr(&cyw43_state.netif[0])), sizeof(sys_web.ip_address_string));   
    STRNCPY(sys_web.network_mask_string, ipaddr_ntoa(netif_ip4_netmask(&cyw43_state.netif[0])), sizeof(sys_web.network_mask_string)); 
    STRNCPY(sys_web.gateway_string, ipaddr_ntoa(netif_ip4_gw(&cyw43_state.netif[0])), sizeof(sys_web.gateway_string)); 

    if (sys->dhcp_enable)
    {
        // set the static network config to match current DHCP assignments
        STRNCPY(sys->ip_address, sys_web.ip_address_string, sizeof(sys->ip_address));
        STRNCPY(sys->network_mask, sys_web.network_mask_string, sizeof(sys->network_mask));        
        STRNCPY(sys->gateway, sys_web.gateway_string, sizeof(sys->gateway));    
    }

    return(0);
}