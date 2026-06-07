
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


//#define DEBUG_UDP_MESSAGES

//#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)





//prototypes


// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

//static variables
void *watchdog_params = NULL;
//char my_program[] = "FOR x = 1 TO 100\nPRINT \"HELLO WORLD! \";X\nNEXT";
//char my_program[] = "5 X = 1\n10 PRINT \"HELLO WORLD! \" + X\n15 X = X +1\n20 GOTO 10";
//char my_program[] = "5 X = 1\n15 X = X +1\n20 GOTO 15";
char my_program[] = "5 X = 1\n10 PRINT \"HELLO WORLD!\" + X\n12 SLEEP 1\n15 X = X +1\n20 GOTO 10";

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
    
    // store passed watchdog parameter 
    watchdog_params = params;

    if (strcasecmp(APP_NAME, "home-controller") == 0)
    {
        // force personality to match single purpose application
        config.personality = HOME_CONTROLLER;
    }    
    
    printf("home controller task started\n");
    while (true)
    {
        basic_Interpreter(NULL, NULL, my_program, sizeof(my_program));

        if ((config.personality == HOME_CONTROLLER))
        {
            //TEST TEST TEST
            // printf("Begin shelly test\n");
            // discover_shelly_devices();
            // printf("End shelly test\n");
            printf("Home Controller\n");
            SLEEP_MS(60000);
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