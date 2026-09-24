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


//extern APP_CONFIG_T config;

// global variable
extern uint32_t unix_time;


extern int daylight_saving_start_month;
extern int daylight_saving_start_day;
extern int daylight_saving_end_month;
extern int daylight_saving_end_day;

// static int irrigation_test_start_mow = -1;




/*!
 * \brief Determine day of week based on date
 * 
 * \param[in]  m  month 1-12
 * \param[in]  d  day 1-31
 * \param[in]  y  year 1970 - 9999
 * \return day of week, 0-6 where 0 = Sunday
 */
int get_day_of_week(int m,int d,int y)
{
    y-=m<3;
    return(y+y/4-y/100+y/400+"-bed=pen+mad."[m]+d)%7;
}

/*!
 * \brief Get day-of-week and minute-of-day in local timezone
 *
 * \param[out]   dow day of week, 0-6 where 0 = Sunday  
 * 
 * \param[out]   mod minute of day, 0 - 1439    
 * 
 * \return 0 on success or -1 on error
 */
int get_dow_and_mod_local_tz(int *dow, int *mod)
{
    int err = 0;
    datetime_t date;
    int day_of_week = 0;
    int minute_of_day = 0;

    // determine daylight savings dates for the current year
    set_daylight_saving_dates();  

    // determine current weekday in UTC
    rtc_get_datetime(&date);
    day_of_week = get_day_of_week(date.month, date.day, date.year);

    // standard time
    minute_of_day = date.hour*MINUTES_IN_HOUR + date.min + sys->timezone_offset; 

    // check for daylight savings
    if (sys->daylightsaving_enable                                                               &&
        ((date.month*31+date.day) >= (daylight_saving_start_month*31+daylight_saving_start_day)) &&
        ((date.month*31+date.day) < (daylight_saving_end_month*31+daylight_saving_end_day)))
    {
        // daylight savings time
        minute_of_day += MINUTES_IN_HOUR; 
    }
    
    // time zone offset means it is the previous day in local time
    if (minute_of_day < 0)
    {
        minute_of_day+= HOURS_IN_DAY*MINUTES_IN_HOUR;

        if (--day_of_week< 0) day_of_week +=DAYS_IN_WEEK;
    }

    // time zone offset means it is the next day in local time
    if (minute_of_day > HOURS_IN_DAY*MINUTES_IN_HOUR)
    {
        minute_of_day -= HOURS_IN_DAY*MINUTES_IN_HOUR;

        if (++day_of_week> 6) day_of_week -=DAYS_IN_WEEK;        
    } 

    if (dow)
    {
      *dow = day_of_week;
    }

    if (mod)
    {
      *mod = minute_of_day;
    }

    return(err);
}



/*!
 * \brief Check if time is between start and end dealing with wrap around 
 *
 * \param[in]  time_mow    0 - 10079 minute of week, beginning Sunday at midnight
 * \param[in]  start_mow   0 - 10079 minute of week, beginning Sunday at midnight
 * \param[in]  end_mow     0 - 10079 minute of week, beginning Sunday at midnight
 * 
 * \return true if in range
 */
bool mow_between(int time_mow, int start_mow, int end_mow)
{
   bool in_range = false;

   // normalize times
   time_mow = time_mow%MINUTES_IN_WEEK;
   start_mow = start_mow%MINUTES_IN_WEEK;
   end_mow = end_mow%MINUTES_IN_WEEK;      

 
   // check for conventional range
   if (start_mow <= end_mow)
   {
      if ((time_mow >= start_mow) && (time_mow < end_mow))
      {
         in_range = true;
      }
   }

   // check for range with wrap at week boundary
   if (start_mow > end_mow)
   {
      if (!(time_mow < end_mow) || (time_mow >= start_mow))
      {
         in_range = true;
      }   
   }

   return(in_range);
}


/*!
 * \brief Calculate future delta between mow times
 *
 * \param[in]  start_mow   0 - 10079 minute of week, beginning Sunday at midnight
 * \param[in]  end_mow     0 - 10079 minute of week, beginning Sunday at midnight
 * 
 * \return future delta i.e. when moving forward in time
 */
int mow_future_delta(int start_mow, int end_mow)
{
   int delta = 0;

   // check for conventional range
   if (start_mow <= end_mow)
   {
      delta = end_mow - start_mow;
   }
   else
   {
      delta = MINUTES_IN_WEEK - start_mow + end_mow;
   }   

   return(delta);
}


/*!
 * \brief Return pointer to string with weekday name give minute of week (mow)
 *
 * \param[in]  day         0 - 6 day of week, beginning Sunday at midnight
 * 
 * \return pointer to string containing name of day or NULL on error
 */
int get_day_from_mow(int mow)
{
   int day;

   day = mow/(24*60);

   CLIP(day, 0 , 6);

   return(day);   
}


/*!
 * \brief Get minute-of-week in local timezone
 *
 * \param[out]   mow minute of week, 0-10079 where 0 = Midnight Sunday    
 * 
 * \return 0 on success or -1 on error
 */
int get_mow_local_tz(int *mow)
{
    int err = 0;
    int dow = 0;
    int mod = 0;

   err = get_dow_and_mod_local_tz(&dow, &mod);

   if(!err)
   {
      *mow = (dow*MINUTES_IN_DAY+mod)%MINUTES_IN_WEEK;
   }

    return(err);
}



/*!
 * \brief Get current time
 *  
 * \return 0 on success, non-zero on error
 */
int8_t get_datetime_from_unix_time(uint32_t unixtime, datetime_t *date, int *effective_offset, int localtime)
{
   int err = 0;
   struct tm * timeinfo;
   time_t t;

   if (date && effective_offset)
   {
      *effective_offset = 0;

      t = unixtime;

      if (localtime)
      {
         // apply timezone offset
         *effective_offset = sys->timezone_offset;
         t += (*effective_offset * 60);
      }

      timeinfo = gmtime(&t);

      memset(date, 0, sizeof(datetime_t));
      date->sec = timeinfo->tm_sec;
      date->min = timeinfo->tm_min;
      date->hour = timeinfo->tm_hour;
      date->day = timeinfo->tm_mday;
      date->month = timeinfo->tm_mon + 1;
      date->year = timeinfo->tm_year + 1900;

      date->dotw = get_day_of_week(date->month, date->day, date->year);

      if (localtime)  // TODO: rather than compute if daylight savings is active use a global flag set by fake rtc
      {
         // check for daylight savings
         if (sys->daylightsaving_enable                                                             &&
            ((date->month*31+date->day) >= (daylight_saving_start_month*31+daylight_saving_start_day)) &&
            ((date->month*31+date->day) < (daylight_saving_end_month*31+daylight_saving_end_day)))
         {
            // apply one hour daylight savings offset
            *effective_offset += 60;
            t += (60*60);         

            // recompute the datetime using the daylight savings offset
            timeinfo = gmtime(&t);

            memset(date, 0, sizeof(datetime_t));
            date->sec = timeinfo->tm_sec;
            date->min = timeinfo->tm_min;
            date->hour = timeinfo->tm_hour;
            date->day = timeinfo->tm_mday;
            date->month = timeinfo->tm_mon + 1;
            date->year = timeinfo->tm_year + 1900;

            date->dotw = get_day_of_week(date->month, date->day, date->year);         
         }
      }
   }
   else
   {
      err = -1;
   }

   return(err);
}




