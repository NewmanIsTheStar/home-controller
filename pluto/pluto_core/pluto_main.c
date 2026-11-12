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


// static variable
static uint32_t reset_reason;

// prototypes
int pluto(void);
int pluto_print_startup_status(void);
void pluto_get_program_size(void);
void print_reset_reason(void);


/*********************************************************
 * THIS MUST BE THE FIRST FUNCTION DEFINED IN THIS FILE!!!
 * *******************************************************/
int __attribute__((__section__(".applet_entry"))) main(void)
{
    #if defined(PICO_RP2350)
    // record the status flags from the RP2350 power manager
    reset_reason = powman_hw->chip_reset;
    #endif

    // launch the executable identified in boot.txt
    //pluto_boot_launch();

    // continue launching the current executable
    pluto();

    return(0); 
}


/*!
 * \brief Initialize free RTOS and launch the boss task 
 *
 * \param none
 *
 * \return 0 if scheduler stops (should never happen)
 */
int pluto(void)
{
    TaskHandle_t task;

    stdio_init_all();

    // print statup messages
    pluto_print_startup_status();

    // initialize boss task
    xTaskCreate(boss_task, "Boss Task", 1024, NULL, BOSS_TASK_PRIORITY, &task);  

    // start boss task
    vTaskStartScheduler();

    printf("Scheduler Exited!\n");

    // we should never get here -- if we do then drop dead
    watchdog_enable(1, 1);
    while(1);

    return 0;
}

/*!
 * \brief print start up status information
 *
 * \param none
 *
 * \return 0 
 */
int pluto_print_startup_status(void)
{
    const char *rtos_name;

    pluto_boot_status();

    print_reset_reason();

    printf("\n%s version ", APP_NAME);

#ifdef USE_GIT_HASH_AS_VERSION
    printf("%s\n\n", GITHASH);
#else
    printf("%s\n\n", PLUTO_VER);
#endif

    pluto_get_program_size();

    printf("Compiled: %s %s\n",__DATE__,__TIME__);
    printf("Pico SDK Version: %s\n\n", PICO_SDK_VERSION_STRING);

    if (watchdog_caused_reboot())
    {
        printf("***Reboot occured***\n");

        switch(pluto_get_reboot_reason())
        {
        default:
        case REBOOT_UNKNOWN:
            printf("Reboot for Unknown reason\n");
            break;
        case REBOOT_SNTP_FAILURE:
            printf("Reboot due to SNTP failure\n");
            break;
        case REBOOT_WEATHER_FAILURE:
            printf("Reboot due to weather failure\n");
            break;
        case REBOOT_USER_REQUEST:
            printf("Reboot due to user request\n");
            break;
        }
    }
    
#if ( portSUPPORT_SMP == 1 )
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if ( portSUPPORT_SMP == 1 ) && ( configNUMBER_OF_CORES == 2 )
    printf("Starting %s on both cores\n", rtos_name);
#elif ( RUN_FREERTOS_ON_CORE == 1 )
    printf("Starting %s on core 1\n", rtos_name);
#else
    printf("Starting %s on core 0\n", rtos_name);
#endif

    return(0);
}

void pluto_get_program_size(void)
{
    int flash_percentage = 0;
    extern char __flash_binary_start;  // defined in linker script
    extern char __flash_binary_end;    // defined in linker script

    uintptr_t start = (uintptr_t) &__flash_binary_start;
    uintptr_t end = (uintptr_t) &__flash_binary_end;
    printf("Binary start: %08x\nBinary end:   %08x\nBinary size:  %08x\n", start, end, end-start);
    flash_percentage = ((end-start)*1000)/(PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE);
    printf("Flash used:   %d.%d%%\n\n", flash_percentage/10, flash_percentage%10);
}

/*!
 * \brief print reason for reset based on hardware register(s)
 * 
 * \return nothing
 */
void print_reset_reason(void)   // TODO: solve for pico_w
{
#if defined(PICO_RP2350)

    printf("Evaluating reset reason...\n");

    // Check for Power-On Reset / Brown-out (Bit 16)
    if (reset_reason & POWMAN_CHIP_RESET_HAD_POR_BITS) {
        printf("Reset Source: Power-On Reset / Voltage Brown-out\n");
    }

    // Check for physical RUN/RSTn pin (Bit 18)
    if (reset_reason & POWMAN_CHIP_RESET_HAD_RUN_LOW_BITS) {
        printf("Reset Source: Physical RUN/RSTn pin pressed\n");
    }

    // Check for external debugger reset request (Bit 21)
    if (reset_reason & POWMAN_CHIP_RESET_HAD_DP_RESET_REQ_BITS) {
        printf("Reset Source: Debugger Request (SWD/SYSRESETREQ)\n");
    }

    // Check for Switched Core Powerdown (Bit 25)
    if (reset_reason & POWMAN_CHIP_RESET_HAD_SWCORE_PD_BITS) {
        printf("Reset Source: Core Powerdown / Deep Sleep Wakeup\n");
    }

    // CRITICAL: Clear the sticky bits so future warm resets don't inherit them.
    // In POWMAN, writing the bits back clears them.
    powman_hw->chip_reset = reset_reason; 
#endif
}









