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
extern uint32_t unix_time;


/*!
 * \brief Generate string containing time stamp from realtime clock
 *
 * \param[out]  timestamp   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * \param[in]   isoformat   use iso format
 * \param[in]   localtime   use local time
 * 
 * \return 0 on success, non-zero on error
 */
int get_timestamp(char *timestamp, int len, int isoformat, int localtime)
{
   int err;

   err = get_timestamp_from_unix_time(unix_time, timestamp, len, isoformat, localtime);

   return(err);
}

/*!
 * \brief Generate string containing local time in human readable format
 *
 * \param[out]  time_string   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * 
 * \return number of characters in the timestamp
 */
int get_local_time_string(char *time_string, int len)
{
   int err = 0;

    err = get_time_string_from_unix_time(unix_time, time_string, len, 0, 1);                

    return(err);   
   
}

/*!
 * \brief print mow time in human readable form e.g. "11:00"
 *
 */
int mow_to_time_string(char *string, int length, int mow)
{
   int hour;
   int minute; 
   int printed = 0;
   
   if ((mow >= 0) && (mow < 60*24*7))
   {
      hour = (mow % (60*24))/60;
      minute = (mow % (60*24))%60;

      CLIP(hour, 0, 23);
      CLIP(minute, 0, 59);

      printed = snprintf(string, length, "%02d:%02d", hour, minute);
   }
   else
   {
      printed = snprintf(string, length, "&nbsp;");  //TODO -- don't like this webUI stuff here
   }

   return(printed);
}

/*!
 * \brief time string to mow
 *
 */
int time_string_to_mow(char *string, int length, int day)
{
   int mow;
   int hour = 0;
   int minute = 0; 
   

   if (isalpha(string[0]))
   {
      if (strcasecmp("sunrise", string) == 0)
      {
         mow = get_sunrise_mod();

      } else if (strcasecmp("sunset", string) == 0)
      {
          mow = get_sunset_mod();
      }
      else
      {
         mow = 0;
      }
   }
   else
   {
      sscanf(string,"%d:%d", &hour, &minute);
      sscanf(string,"%d%%3A%d", &hour, &minute);   

      //printf(">>>>>AFTER SCANF %d %d\n", hour, minute);

      CLIP(day, 0, 6);
      CLIP(hour, 0, 23);
      CLIP(minute, 0, 59);

      mow = day*24*60 + hour*60 + minute;
   }

   return(mow);
}

/*!
 * \brief DO NOT USE! Get current time  --obsolete and does not account for daylight savings--
 *  
 * \return 1
 */
int8_t get_datetime(datetime_t *date, int localtime)
{
   struct tm * timeinfo;
   time_t t;

   t = unix_time; // must be atomic

   if (localtime)
   {
      // apply timezone offset
      t += (sys->timezone_offset * 60); 
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

   return(1);
}

/*!
 * \brief Generate string containing time stamp from unix time
 *
 * \param[in]   unixtime    unix time  
 * \param[out]  timestamp   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * \param[in]   isoformat   use iso format
 * \param[in]   localtime   use local time
 * 
 * \return 0 on success, non-zero on error
 */
int get_timestamp_from_unix_time(uint32_t unixtime, char *timestamp, int len, int isoformat, int localtime)
{
   int err = 0;
   datetime_t date;
   char offset_string[12];
   int effective_offset = 0;


   err = get_datetime_from_unix_time(unixtime, &date, &effective_offset, localtime);      

   if (!err)
   {
      if (!localtime || (effective_offset == 0)) 
      {
         // zulu time
         if (isoformat)
         {
            sprintf(offset_string, "Z");
         }
         else
         {
            sprintf(offset_string, "");
         }
      }
      else
      {
         sprintf(offset_string, "%c%02d:%02d", effective_offset<0?'-':'+', abs(effective_offset/60), abs(effective_offset%60));
      }

      if (isoformat)
      {
         // iso format needed for syslog
         snprintf(timestamp, len, "%04d-%02d-%02dT%02d:%02d:%02d.000%s", date.year, date.month, date.day, date.hour, date.min, date.sec, offset_string);
      }
      else
      {
        // human readable format
        snprintf(timestamp, len, "%04d-%02d-%02d %02d:%02d:%02d UTC%s", date.year, date.month, date.day, date.hour, date.min, date.sec, offset_string);
      }
    }
    else
    {
        snprintf(timestamp, len, "1970-01-01T00:00:000.000Z");  //default to unix epoch
    }

    return(err);
}

/*!
 * \brief Generate string containing time stamp from unix time
 *
 * \param[in]   unixtime    unix time  
 * \param[out]  timestamp   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * \param[in]   isoformat   use iso format
 * \param[in]   localtime   use local time
 * 
 * \return 0 on success, non-zero on error
 */
int get_local_timestamp_from_unix_time(uint32_t unixtime, char *timestamp, int len)
{
   int err = 0;
   datetime_t date;
   int effective_offset = 0;


   err = get_datetime_from_unix_time(unixtime, &date, &effective_offset, 1);      

   if (!err)
   {
      // human readable format
      snprintf(timestamp, len, "%04d-%02d-%02d %02d:%02d:%02d", date.year, date.month, date.day, date.hour, date.min, date.sec);
      
    }
    else
    {
        snprintf(timestamp, len, "1970-01-01T00:00:000.000Z");  //default to unix epoch
    }

    return(err);
}

/*!
 * \brief Generate string containing time delta in human readable format from integer delta in seconds
 *
 * \param[in]   string        output buffer  
 * \param[out]  len           size of output buffer 
 * \param[in]   delta_seconds delta in seconds  
 * 
 * \return number of characters written to output buffer
 */
int get_delta_string_from_delta_seconds(char *string, int len, uint32_t delta_seconds)
{
   int years = 0;
   int months = 0;
   int weeks = 0;
   int days = 0;
   int hours = 0;
   int minutes = 0;
   int seconds = 0;
   int remaining = 0;
   char temp_string[32];
   int printed = 0;

   if (string && len)
   {
      // terminate
      string[0] = 0;

      // initialize remaining seconds
      remaining = delta_seconds;
      
      // years
      years = remaining/31556926;
      remaining -= years*31556926;

      // months
      months = remaining/2629800;
      remaining -= months*2629800;

      // days
      days = remaining/86400;
      remaining -= days*86400;

      // hours
      hours = remaining/3600;
      remaining -= hours*3600;

      // minutes
      minutes = remaining/60;
      remaining -= minutes*60;

      // seconds
      seconds = remaining;

      if (years)
      {
         printed += snprintf(temp_string, sizeof(temp_string), "%d year%c", years, years==1?'\0':'s');
         STRNCAT(string, temp_string, len);
      }

      if (years || months)
      {
         printed += snprintf(temp_string, sizeof(temp_string), " %d month%c", months, months==1?'\0':'s');
         STRNCAT(string, temp_string, len);
      }   
      
      if (years || months || days)
      {
         printed += snprintf(temp_string, sizeof(temp_string), " %d day%c", days, days==1?'\0':'s');
         STRNCAT(string, temp_string, len);
      }        

      if (years || months || days || hours)
      {
         printed += snprintf(temp_string, sizeof(temp_string), " %d hour%c", hours, hours==1?'\0':'s');
         STRNCAT(string, temp_string, len);
      }

      if (years || months || days || hours || minutes)
      {
         printed += snprintf(temp_string, sizeof(temp_string), " %d minute%c", minutes, minutes==1?'\0':'s');
         STRNCAT(string, temp_string, len);
      }   
      
      if (years || months || days || hours || minutes || seconds)
      {
         printed += snprintf(temp_string, sizeof(temp_string), " %d second%c", seconds, seconds==1?'\0':'s');
         STRNCAT(string, temp_string, len);
      }       
   }   

    return(printed);
}

/*!
 * \brief Generate string containing date from unix time
 *
 * \param[in]   unixtime    unix time  
 * \param[out]  date_string pointer to string to store the date 
 * \param[in]   len         max length of date_string  
 * \param[in]   isoformat   use iso format
 * \param[in]   localtime   use local time
 * 
 * \return 0 on success, non-zero on error
 */
int get_date_string_from_unix_time(uint32_t unixtime, char *date_string, int len, int isoformat, int localtime)
{
   int err = 0;
   datetime_t date;
   char offset_string[12];
   int effective_offset = 0;


   err = get_datetime_from_unix_time(unixtime, &date, &effective_offset, localtime);      

   if (!err)
   {
      if (isoformat)
      {
         snprintf(date_string, len, "%04d-%02d-%02d", date.year, date.month, date.day);            
      }
      else if (sys->use_archaic_units)
      {
         snprintf(date_string, len, "%02d/%02d/%04d", date.month, date.day, date.year);         
      }
      else
      {
         snprintf(date_string, len, "%02d/%02d/%04d", date.day, date.month, date.year);  
      }
    }
    else
    {
        snprintf(date_string, len, "1970-01-01T00:00:000.000Z");  //default to unix epoch
    }

    return(err);
}

/*!
 * \brief Generate string containing local date in human readable format
 *
 * \param[out]  timestamp   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * 
 * \return number of characters in the timestamp
 */
int get_local_date_string(char *date_string, int len)
{
   int err = 0;

    err = get_date_string_from_unix_time(unix_time, date_string, len, 0, 1);                   

    return(err);
}

/*!
 * \brief Generate string containing date from unix time
 *
 * \param[in]   unixtime    unix time  
 * \param[out]  day_string  pointer to string to store the day 
 * \param[in]   len         max length of date_string  
 * \param[in]   isoformat   use iso format
 * \param[in]   localtime   use local time
 * 
 * \return 0 on success, non-zero on error
 */
int get_day_string_from_unix_time(uint32_t unixtime, char *day_string, int len, int isoformat, int localtime)
{
   int err = 0;
   datetime_t date;
   char offset_string[12];
   int effective_offset = 0;
   int day = 0;


   err = get_datetime_from_unix_time(unixtime, &date, &effective_offset, localtime);      

   if (!err)
   {
      day = get_day_of_week(date.month, date.day, date.year);

      snprintf(day_string, len, "%s", day_name(day));
   }

    return(err);
}

/*!
 * \brief Generate string containing local date in human readable format
 *
 * \param[out]  timestamp   pointer to string to store the timestamp 
 * \param[in]   len         max length of timestamp string  
 * 
 * \return number of characters in the timestamp
 */
int get_local_day_string(char *day_string, int len)
{
   int err = 0;

    err = get_day_string_from_unix_time(unix_time, day_string, len, 0, 1);                   

    return(err);
}

/*!
 * \brief Generate string containing date from unix time
 *
 * \param[in]   unixtime    unix time  
 * \param[out]  date_string pointer to string to store the date 
 * \param[in]   len         max length of date_string  
 * \param[in]   isoformat   use iso format
 * \param[in]   localtime   use local time
 * 
 * \return 0 on success, non-zero on error
 */
int get_time_string_from_unix_time(uint32_t unixtime, char *time_string, int len, int isoformat, int localtime)
{
   int err = 0;
   datetime_t date;
   int effective_offset = 0;

   err = get_datetime_from_unix_time(unixtime, &date, &effective_offset, localtime);      

   if (!err)
   {
      if (localtime)
      {
         // time only
         //snprintf(time_string, len, "%02d:%02d:%02d", date.hour, date.min, date.sec);
         snprintf(time_string, len, "%02d:%02d", date.hour, date.min);         
      }
      else
      {
        // add UTC
        //snprintf(time_string, len, "%02d:%02d:%02d UTC", date.hour, date.min, date.sec);
        snprintf(time_string, len, "%02d:%02d UTC", date.hour, date.min);        
      }
    }
    else
    {
        snprintf(time_string, len, "1970-01-01T00:00:000.000Z");  //default to unix epoch
    }

    return(err);
}

#ifdef USURPER
/*!
 * \brief Get start and stop times for next irrigation period
 *
 * \param[out]  start_mow   0 - 10079 minute of week, beginning Sunday at midnight
 * \param[out]  end_mow     0 - 10079 minute of week, beginning Sunday at midnight
 * \param[out]  delay_mins  0 - 10079 minutes from now until next irrigation period
 * \param[out]  zone        0 - 15
 * 
 * \return -1 on error, 0 on success, 1 if currently in irrigation period
 */
SCHEDULE_QUERY_STATUS_LT get_next_irrigation_period(int *start_mow, int *end_mow, int *delay_mins, int *zone)
{
   int potential_zone;
   int day;      
   int now_mow = 0;
   int irrigate_now = SCHEDULE_NEVER;
   int candidate_start_mow;
   int candidate_end_mow;
   int lowest_delta = MINUTES_IN_WEEK;
   int delta = 0;
   int day_total_duration = 0;

   // get current minute of week 
   get_mow_local_tz(&now_mow);                      

    // search for next irrigation period
   for (day = 0; (day < DAYS_IN_WEEK) && (irrigate_now != SCHEDULE_NOW); day++)
   {
      if (cfg->day_schedule_enable[day])
      {
         day_total_duration = 0;

         for(potential_zone=0; potential_zone < cfg->zone_max; potential_zone++)
         {
            if (cfg->zone_enable[potential_zone])
            {   
               candidate_start_mow = (day*HOURS_IN_DAY*MINUTES_IN_HOUR + cfg->day_start[day] + day_total_duration)%MINUTES_IN_WEEK;
               candidate_end_mow =   (candidate_start_mow + cfg->zone_duration[potential_zone][day])%MINUTES_IN_WEEK;

               // maintain running total of duration for the day
               day_total_duration += cfg->zone_duration[potential_zone][day];

               // check if current time is within the candidate irrigation period
               if (mow_between(now_mow, candidate_start_mow, candidate_end_mow))
               {
                  irrigate_now = SCHEDULE_NOW;
                  *start_mow = candidate_start_mow;
                  *end_mow = candidate_end_mow;
                  *delay_mins = 0;
                  *zone = potential_zone;
                  break;
               }

               // find the lowest delta to a future irrigation
               delta = mow_future_delta(now_mow, candidate_start_mow);
               if (delta < lowest_delta)
               {
                  irrigate_now = SCHEDULE_FUTURE;                  
                  lowest_delta = delta;                  
                  *start_mow = candidate_start_mow;
                  *end_mow = candidate_end_mow;
                  *delay_mins = delta;
                  *zone = potential_zone;
               }            
            }
         }
      }   
   }

   return (irrigate_now);
}
#endif