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
// extern bool restart_requested;
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
 * \brief GPIO defaults for pins that are unused but connected to hardware
 *
 * \param none
 *
 * \return 0
 */
int set_gpio_defaults(void)
{
    int i;

    for(i=0; i<NUM_ROWS(sys->gpio_default); i++)
    {
        switch(sys->gpio_default[i])
        {
        default:
        case GP_UNINITIALIZED:
            break;
        case GP_INPUT_FLOATING:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            printf("GPIO %d :: input floating\n", i);            
            break;
        case GP_INPUT_PULLED_HIGH:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            gpio_set_pulls (i, true, false);
            printf("GPIO %d :: input internally pulled high\n", i);             
            break;
        case GP_INPUT_PULLED_LOW:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            gpio_set_pulls (i, false, true);
            printf("GPIO %d :: input internally pulled low\n", i);             
            break;
        case GP_OUTPUT_HIGH:
            gpio_init(i);
            gpio_set_dir (i, true);
            gpio_put (i, true);
            printf("GPIO %d :: output high\n", i); 
            break;
        case GP_OUTPUT_LOW:
            gpio_init(i);
            gpio_set_dir (i, true);
            gpio_put (i, false);
            printf("GPIO %d :: output low\n", i);
            break;            
        }
    }
}
