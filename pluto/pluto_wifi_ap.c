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
// extern REBOOT_REASON_T __uninitialized_ram(reboot_reason);

// external variables
extern uint32_t unix_time;
extern APP_CONFIG_T config;
extern SYS_WEB_VARIABLES_T sys_web;
extern WORKER_TASK_T worker_tasks[];

// // static variables
extern bool restart_requested;
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
 * \brief AP mode for initial configuration 
 *
 * \param none
 *
 * \return 0
 */
int pluto_ap_mode(void)
{
    bool led_on = false;
    int ap_idle = 0;
    ip_addr_t gw;
    ip4_addr_t mask;
    dhcp_server_t dhcp_server;
    dns_server_t dns_server;    



    printf("Initializing AP mode\n");
    cyw43_arch_enable_ap_mode("pluto", "",	CYW43_AUTH_OPEN); 

    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    printf("Connect to WiFi network called: pluto\nPoint web browser to http://192.168.4.1\n");

    // Start the dhcp server
    dhcp_server_init(&dhcp_server, &gw, &mask);

    // Start the dns server
    dns_server_init(&dns_server, &gw);

    // start web server to allow user to configure wifi
    system_web_init();
    httpd_init();
    ssi_init();
    cgi_init();	

    sys_web.access_point_mode = true;

    // flash the led fast to indicate AP mode
    while(true)
    {
        // toggle led
        cyw43_arch_gpio_put(0, led_on);
        led_on = !led_on;
        
        // tell watchdog task that we are alive
        //watchdog_pulse();  // trade off -- allow more time before regular watchdog reboot vs. risk of never rebooting

        if (config_dirty(false))
        {
            //config_write(CONFIG_FILE);
            config_write();

            //user is changing configuration
            ap_idle = 0;
        }

        // count idle cycles in AP mode
        ap_idle++;

        // reboot if in AP mode for 10 minutes without user altering configuration
        if (ap_idle > 10*60*10)
        {
            cyw43_arch_disable_ap_mode();
            cyw43_arch_deinit();
            sys_web.access_point_mode = false;
            watchdog_enable(1, 0);
        }
        else
        {
            // pat the watchdog
            //watchdog_update();  // trade off -- allow more time before regular watchdog reboot vs. risk of never rebooting
        }
        
        SLEEP_MS(100);

        if (restart_requested)
        {
            config_write();
            
            printf("***REBOOT in 100 ms***\n");
            cyw43_arch_disable_ap_mode();
            cyw43_arch_deinit();

            sys_web.access_point_mode = false;
            SLEEP_MS(100);
            
            watchdog_enable(1, 0);
        }
    } 

    return(0); 
}    

/*!
 * \brief AP mode used for development without access to a wifi network
 *
 * \param none
 *
 * \return 0
 */
int pluto_ap_mode_for_developers(void)
{
    bool led_on = false;
    int ap_idle = 0;
    static ip_addr_t gw;
    static ip4_addr_t mask;
    static dhcp_server_t dhcp_server;
    static dns_server_t dns_server;    

    printf("Initializing AP mode\n");
    cyw43_arch_enable_ap_mode("pluto", "",	CYW43_AUTH_OPEN); 

    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    printf("Connect to WiFi network called: pluto\nPoint web browser to http://192.168.4.1\n");

    // Start the dhcp server
    dhcp_server_init(&dhcp_server, &gw, &mask);

    // Start the dns server
    dns_server_init(&dns_server, &gw);

    return(0);
}