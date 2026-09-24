/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "lwip/sockets.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

//#include "weather.h"
//#include "led_strip.h"
#include "cgi.h"
#include "ssi.h"

#include "utility.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"
#include "watchdog.h"
#include "worker_tasks.h"
#include "pluto.h"

// external variables
extern WORKER_TASK_T worker_tasks[];
extern WEB_VARIABLES_T web;

/*!
 * \brief Tell watchdog taskdog task that we are alive!
 *
 * \param alive poimter to alive inidcation to be altered
 * 
 * \return nothing
 */
void watchdog_pulse(int *alive) 
{
    (*alive)++;  //atomic write not necessary as all the watchdog task needs to see is a changing value
}


/*!
 * \brief Monitor for task crashes and pat the watchdog
 *
 * \param params unused garbage
 *
 * \return nothing
 */
void watchdog_task(void *params)
{
    int worker = 0;
    int reset_required = false;
    
    // start the watchdog
    watchdog_enable(5000, 1);

    // pat the watchdog
    while(true) 
    {
        // scan worker tasks to check if alive
        for(worker=0; worker_tasks[worker].functionptr != NULL; worker++)
        {
            if (worker_tasks[worker].watchdog_alive_indicator)      
            {
                worker_tasks[worker].watchdog_alive_indicator = 0;  
                worker_tasks[worker].watchdog_seconds_since_alive = 0;
            }
            else
            {
                worker_tasks[worker].watchdog_seconds_since_alive++;
                if (worker_tasks[worker].watchdog_seconds_since_alive > 90)
                {

                    printf("%s seconds_since_alive == %d\n", worker_tasks[worker].name, worker_tasks[worker].watchdog_seconds_since_alive);
                }

                if (worker_tasks[worker].watchdog_seconds_since_alive > 180) 
                {
                    reset_required = true;
                }
            }
        }

#ifdef DEBUG_DEAD_TASK
        // keep printing seconds since alive message if worker task dies
        watchdog_update();
#else
        // update watchdog if task still alive
        if (!reset_required)
        {
            watchdog_update();
        }
        else
        {
            application_restart(REBOOT_WATCHDOG);
        }
#endif
        SLEEP_MS(1000);
    }
}

/*!
 * \brief Log watchdog reset if it occured
 * This function may be called multiple times.  It will do nothing once it has 
 * successfully sent a log message to the syslog server.  The intent is to ensure 
 * that watchdog reboots are logged even if the syslog server was not available
 * when the system started.
 * \return socket or -1 on error
 */
int check_watchdog_reboot(void)
{
    static int watchdog_reset = -1;
    static bool syslog_sent = false;
    static bool web_page_updated = false;

    if (watchdog_reset < 0)
    {
        // cache watchdog reset status
        watchdog_reset = watchdog_caused_reboot();
    }
    
    if (watchdog_reset && !web_page_updated)
    {
        // update web page
        get_timestamp(web.watchdog_timestring, sizeof(web.watchdog_timestring), false, true); 
        web_page_updated = true;  
    }

    if (watchdog_reset && sys->syslog_enable && !syslog_sent)
    {
        // log watchdog event
        if ((send_syslog_message("usurper", "REBOOT @ %s [reason = %lu]", web.watchdog_timestring, get_reboot_reason())) > 0)   
        {
            syslog_sent = true;
        }
    }    

    return(0);
}