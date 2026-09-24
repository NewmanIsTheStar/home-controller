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
uint32_t unix_time = 0;
uint32_t unix_time_delta_in_ticks = 0;
long int sntp_update_counter = 0;


/*!
 * \brief Set fake RTC -- called by lwip sntp code when a time update is received
 *
 */
int8_t rtc_set_datetime(uint32_t sec)
{
   // try to prevent time going backwards   
   if (sec >= unix_time)
   {
      unix_time = sec; // must be atomic!
   }
   else
   {
      // our local time is ahead of ntp so record delta in ticks
      unix_time_delta_in_ticks = (unix_time - sec) *1000;

      // if delta is huge a step change is necessary
      if (unix_time_delta_in_ticks > 60000)
      {
         unix_time = sec; // time goes backwards!
         unix_time_delta_in_ticks = 0;
      }
   }

   sntp_update_counter++;   // used to monitor sntp connectivity

   return(1);
}


/*!
 * \brief Update fake rtc  -- this should be called at least once per second to maintain unix_time between sntp updates
 *
 * This should only be called from one task!
 */
uint32_t rtc_update(void)
{
   TickType_t current_tick;
   TickType_t increment;
   static TickType_t last_tick = 0;
   static bool initialized = false;   

   current_tick = xTaskGetTickCount();

   if(!initialized)
   {
      last_tick = current_tick;
      initialized = true;
   }
   else
   {
      increment = (current_tick - last_tick);

      // check if clock shaving required
      if (unix_time_delta_in_ticks)
      {
         if (increment > 200)
         {
            if (unix_time_delta_in_ticks > 100)
            {
               // shave 100 ms
               increment -= 100;  
               unix_time_delta_in_ticks -= 100;
            } 
            else if (unix_time_delta_in_ticks > 0)
            {
               // shave remaining ms
               increment -= unix_time_delta_in_ticks;  
               unix_time_delta_in_ticks = 0;            
            }    
         }
      }
           
      increment /= 1000;
      last_tick += increment*1000;   // avoid accumulating rounding errors
      unix_time += increment;        // must be atomic!
   }

   return(unix_time_delta_in_ticks);
}

/*!
 * \brief Get seconds part of the current real time
 *
 * \param[out]   dow day of week, 0-6 where 0 = Sunday  
 * 
 * \param[out]   mod minute of day, 0 - 1439    
 * 
 * \return 0-59 on success or 0 if unable to read rtc
 */
int8_t rtc_get_datetime(datetime_t *date)
{
   struct tm * timeinfo;
   time_t t;

   t = unix_time; // TODO make atomic

	timeinfo = gmtime(&t);

	memset(date, 0, sizeof(datetime_t));
	date->sec = timeinfo->tm_sec;
	date->min = timeinfo->tm_min;
	date->hour = timeinfo->tm_hour;
	date->day = timeinfo->tm_mday;
	date->month = timeinfo->tm_mon + 1;
	date->year = timeinfo->tm_year + 1900;

   date->dotw = get_day_of_week(date->month, date->day, date->year);

   return(1);
}


/*!
 * \brief check sntp is receiving updates
 *
 */
bool sntp_alive(void)
{
   bool alive = true;
   static long int poll_counter = 0;
   static long int last_sntp_update_counter= 0;
   
   poll_counter++;

   if (sntp_update_counter != last_sntp_update_counter)
   {
      printf("sntp updates: %d @ poll number %d\n", sntp_update_counter, poll_counter);
      // an sntp update has occured since the last poll so reset counter
      poll_counter = 0;
      last_sntp_update_counter = sntp_update_counter;
   }
   else if (poll_counter > 60*60*24)
   {
      alive = false;
   }

   return(alive);
}

/*!
 * \brief Get seconds part of the current real time
 *
 * \param[out]   dow day of week, 0-6 where 0 = Sunday  
 * 
 * \param[out]   mod minute of day, 0 - 1439    
 * 
 * \return 0-59 on success or 0 if unable to read rtc
 */
int8_t get_real_time_clock_seconds(void)
{
   datetime_t date;

   date.sec = 0;
   rtc_get_datetime(&date);

   return(date.sec);
}
