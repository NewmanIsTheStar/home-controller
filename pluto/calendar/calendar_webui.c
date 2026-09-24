/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// ACRONYMS
// MOD = Minute of Day  :: range 0 to 1439 where 0 = midnight
// MOW = Minute of Week :: range 0 to 10079 where 0 = midnight on the first day of the week (user configurable as either Sunday or Monday)

#define _GNU_SOURCE

#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
#include <string.h>
//#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"

#include "lwip/sockets.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

//#include "weather.h"
#include "calendar.h"
#include "cgi.h"


#include "config.h"
#include "system_config.h"
#include "application_config.h"
#include "pluto.h"
#include "ssi.h"
#include "time.h"
#include "utility.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"

#include <stdio.h>
#include <math.h>



// global variable
char current_calendar_web_page[50] = "/landscape.shtml";


/*!
 * \brief Set a default time server in config if all four time server entries are blank
 * 
 * \return 0 on success, -1 on error
 */
int calendar_timeserver_failsafe(void)
{
    // failsafe - if no timeserver configured try pool.ntp.org
    if ((sys->time_server[0][0] == 0) &&
        (sys->time_server[1][0] == 0) &&
        (sys->time_server[2][0] == 0) &&
        (sys->time_server[3][0] == 0))
    {
        STRNCPY(sys->time_server[0], "pool.ntp.org", sizeof(sys->time_server[0]));
    }

    return(0);
}

/*!
 * \brief Set name of html page to use to disaplay one week calendar  
 * 
 * \return 0 on success or -1 on error
 */
int set_calendar_html_page(void)
{
   switch(sys->personality)
   {
   default:
   case NO_PERSONALITY:
         STRNCPY(current_calendar_web_page, "/personality.shtml", sizeof(current_calendar_web_page));    
         break;
   case SPRINKLER_USURPER:
         if (sys->use_monday_as_week_start)
         {
            STRNCPY(current_calendar_web_page, "/landscape_monday.shtml", sizeof(current_calendar_web_page));            
         }
         else
         {
            STRNCPY(current_calendar_web_page, "/landscape.shtml", sizeof(current_calendar_web_page)); 
         }
         break;
   case SPRINKLER_CONTROLLER:
         if (sys->use_monday_as_week_start)
         {
            STRNCPY(current_calendar_web_page, "/zm_landscape.shtml", sizeof(current_calendar_web_page)); 
         }
         else
         {
            STRNCPY(current_calendar_web_page, "/zs_landscape.shtml", sizeof(current_calendar_web_page)); 
         }
         break;                
   case LED_STRIP_CONTROLLER:
         STRNCPY(current_calendar_web_page, "/led_controller.shtml", sizeof(current_calendar_web_page)); 
         break;
   case HVAC_THERMOSTAT:
         if (sys->use_monday_as_week_start)
         {
            STRNCPY(current_calendar_web_page, "/tm_thermostat.shtml", sizeof(current_calendar_web_page));           
         }
         else
         {
            STRNCPY(current_calendar_web_page, "/ts_thermostat.shtml", sizeof(current_calendar_web_page)); 
         }         
         break;         
   }


    return(0);
}