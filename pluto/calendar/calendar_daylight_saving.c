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



// prototypes
int get_keyword_index(char *sentence, const char *keywords[]);
int get_day_of_month(int year, int month, int weekday, int instance_of_weekday);
int mow_to_string(char *string, int length, int mow);
int string_to_mow(char *string, int length);


// extern variable
extern uint32_t unix_time;


int daylight_saving_start_month;
int daylight_saving_start_day;
int daylight_saving_end_month;
int daylight_saving_end_day;
// static int irrigation_test_start_mow = -1;


const char *weekdays[] =
{
   "Sunday",
   "Monday",
   "Tuesday",
   "Wednesday",
   "Thursday",
   "Friday",
   "Saturday",
   NULL
};

const char *months[] =
{
   "January",
   "February",
   "March",
   "April",
   "May",
   "June",
   "July",
   "August",
   "September",
   "October",
   "November",
   "Decemeber",
   NULL
};

const char *instances[] =
{
   "First",
   "Second",
   "Third",
   "Fourth",
   "Last",
   NULL
};


/*!
 * \brief Find keyword in sentence
 *
 * \param[in]  sentence   pointer to string to search for keywords
 * \param[in]  keywords   array of pointers to keywords
 * 
 * \return index of first keyword found or -1 if no keyword found
 */
int get_keyword_index(char *sentence, const char *keywords[])
{
   int i = 0;
   int found = -1;

   while(keywords[i])
   {
      if (strcasestr(sentence, keywords[i]))
      {
         found = i;
         break;
      }

      i++;
   }

   return(found);
}

/*!
 * \brief Find day of month 1-31 corresponding to the year, month, and weekday instance e.g. 2nd Tuesday in March 1970
 *
 * \param[in]  year                 year 1970-9999
 * \param[in]  month                month 0-11
 * \param[in]  weekday              weekday 0-6
 * \param[in]  instance_of_weekday  occurence of weekday 1-4
 * 
 * \return day of month or -1 if not found
 */
int get_day_of_month(int year, int month, int weekday, int instance_of_weekday)
{
   int day;
   int max_days;
   int found = 0;

   switch (month)
   {
      case 1: //February
         max_days = 29;  //TODO: normal vs. leap year
         break;

      case 3: //April
      case 5: //June
      case 8: //September
      case 10: //November
         max_days = 30;
         break;

      case 0: //January
      case 2: //March
      case 4: //May
      case 6: //July
      case 7: //August
      case 9: //October
      case 11: //December
      default:
         max_days = 31;
         break;         
   }

   // shift from zero based index to one based numbering used to compute day of week
   month++; 

   if (instance_of_weekday == 4)  // special case -- last occurence of weekday
   {
      //search backwards for last time weekday occurs in month
      for (day=max_days; day > 0; day--)
      {
         if (get_day_of_week(month, day, year) == weekday)   // weekday is zero based, 0 = sunday
         {
              found = 1;
              break;
         }
      }
   }
   else
   {
      // search forwards for first, second, third and fourth weekday occurences
      instance_of_weekday++;  // shift from zero based index to countdown

      for (day=1; day <=max_days; day++)
      {
         if (get_day_of_week(month, day, year) == weekday)   // weekday is zero based, 0 = sunday
         {
            if (--instance_of_weekday == 0)
            {
               found = 1;
               break;
            }
         }
      }
   }

   if (!found) day = -1;

   return(day);
}

/*!
 * \brief Determine daylight saving start and end dates
 * 
 * \return 0 on success, -1 on error
 */
int set_daylight_saving_dates(void)
{
   int err = 0;
   int year;
   datetime_t t;   
   int ok = 0;

   // determine current year
   ok = rtc_get_datetime(&t);

    if (ok)
    {
      year = t.year;

      err = get_daylight_saving_month_and_day(year, sys->daylightsaving_start, &daylight_saving_start_month, &daylight_saving_start_day);

      if (!err)
      {
         err = get_daylight_saving_month_and_day(year, sys->daylightsaving_end, &daylight_saving_end_month, &daylight_saving_end_day);
      }
    }
    else
    {
      err = -1;
    }

   if(err)
   {
      // on error disable daylight saving date range
      daylight_saving_start_month = 0;
      daylight_saving_start_day = 0;
      daylight_saving_end_month = 0;
      daylight_saving_end_day = 0;
   }

   return(err);
}

/*!
 * \brief Extract daylight savings day/month from text description for the given year
 * 
 * \param[in]  year                    year 1970 - 9999
 * \param[in]  date_description        e.g. "First Sunday in March"
 * \param[out] daylight_savings_month  month 0-11
 * \param[out] daylight_savings_day    day 0-6
 * \return 0 on success, -1 on error
 */
int get_daylight_saving_month_and_day(int year, char *date_description, int *daylight_savings_month, int *daylight_savings_day)
{
   int err = -1;
   int month;
   int weekday;
   int instance_of_weekday;
   int day_of_month;  

   CLIP(year, 1970, 9999);

   // extract keywords from sentence describing start of daylight saving
   month = get_keyword_index(date_description, months);
   weekday = get_keyword_index(date_description, weekdays);
   instance_of_weekday = get_keyword_index(date_description, instances);


   if ((month >= 0) && (weekday >= 0) && (instance_of_weekday >= 0))
   {
      day_of_month = get_day_of_month(year, month, weekday, instance_of_weekday);


      if (day_of_month > 0)
      {
         // set daylight saving start
         *daylight_savings_month = month+1;
         *daylight_savings_day = day_of_month;

         err = 0;
      }      
   }

   return(err);
}

/*!
 * \brief Normalize text description of daylight savings start/end amd remove extraneous words
 *
 * \param[in]  in       daylight savings description e.g. "The first Sunday in March"
 * \param[in]  out      daylight savings description normalized e.g. "First Sunday in March"
 * \param[in]  len      maximum length of output string
 * 
 * \return 0 on success
 */
int sanitize_daylight_saving_date(char *in, char *out, int len)
{
   int month;
   int weekday;
   int instance_of_weekday;

   // extract keywords from sentence describing start of dayling saving
   month = get_keyword_index(in, months);
   weekday = get_keyword_index(in, weekdays);
   instance_of_weekday = get_keyword_index(in, instances);


   if ((month >= 0) && (weekday >= 0) && (instance_of_weekday >= 0))
   {
      snprintf(out, len, "%s %s in %s", instances[instance_of_weekday], weekdays[weekday], months[month]);
   }
   else
   {
       snprintf(out, len, "Unknown");
   }

   return(0);
}





/*!
 * \brief Determine if date is within daylight savings period
 *
 * \param[in]    date datetime_t structure with date to check   
 * 
 * \return true if in range otherwise false
 */
int daylight_savings_active(datetime_t date)
{
   bool daylight_savings = false;

    if (sys->daylightsaving_enable                                                             &&
        ((date.month*31+date.day) >= (daylight_saving_start_month*31+daylight_saving_start_day)) &&
        ((date.month*31+date.day) < (daylight_saving_end_month*31+daylight_saving_end_day)))
   {
      daylight_savings = true;
   }

   return(daylight_savings);
}




/*!
 * \brief Return pointer to string with weekday name
 *
 * \param[in]  day         0 - 6 day of week, beginning Sunday at midnight
 * 
 * \return pointer to string containing name of day or NULL on error
 */
const char *day_name(int day)
{
   const char *name = NULL;

   if ((day >=0) && (day <DAYS_IN_WEEK))
   {
      name = weekdays[day];
   }

   return(name);
}



/*!
 * \brief print mow in human readable form e.g. "Monday 11:00"
 *
 */
int mow_to_string(char *string, int length, int mow)
{
   int day;
   int hour;
   int minute; 
   int printed = 0;
   
   day = mow / (60*24);
   hour = (mow % (60*24))/60;
   minute = (mow % (60*24))%60;

   CLIP(day, 0, 6);
   CLIP(hour, 0, 23);
   CLIP(minute, 0, 59);

   printed = snprintf(string, length, "%s %02d:%02d", weekdays[day], hour, minute);

   return(printed);
}



/*!
 * \brief string to mow
 *
 */
int string_to_mow(char *string, int length)
{
   int mow;
   int day = 0;
   int hour = 0;
   int minute = 0; 

   for(day = 0; day < 6; day++)
   {
      if (strcasestr(string, weekdays[day]))
      {
         string += strlen(weekdays[day]);
         break;
      }
   }
   
   sscanf(string,"%d:%d", &hour, &minute);
   sscanf(string,"%d%%3A%d", &hour, &minute);     

   CLIP(day, 0, 6);
   CLIP(hour, 0, 23);
   CLIP(minute, 0, 59);

   mow = day*24*60 + hour*60 + minute;

   return(mow);
}

