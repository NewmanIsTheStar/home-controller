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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


extern uint32_t unix_time;

// global variables

double degToRad(double degrees) {
    return degrees * M_PI / 180.0;
}

double radToDeg(double radians) {
    return radians * 180.0 / M_PI;
}

// Determines if a year is a leap year
int isLeapYear(int year) {
    if (year % 400 == 0) return 1;
    if (year % 100 == 0) return 0;
    return (year % 4 == 0);
}

// Converts a YYYY-MM-DD string into the sequential day of the year
int calculateDayOfYear(const char* dateStr, int* valid) {
    int year, month, day;
    
    // Parse the YYYY-MM-DD format
    if (sscanf(dateStr, "%d-%d-%d", &year, &month, &day) != 3) {
        *valid = 0;
        return -1;
    }
    
    // Validate months and days bounds roughly
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        *valid = 0;
        return -1;
    }
    
    int daysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Adjust for leap year February
    if (isLeapYear(year)) {
        daysInMonths[1] = 29;
    }
    
    // Specific day bound check for parsed month
    if (day > daysInMonths[month - 1]) {
        *valid = 0;
        return -1;
    }
    
    // Accumulate days
    int dayOfYear = 0;
    for (int i = 0; i < month - 1; i++) {
        dayOfYear += daysInMonths[i];
    }
    dayOfYear += day;
    
    *valid = 1;
    return dayOfYear;
}

void formatTime(double minutesFromMidnight, double timeZoneOffset, char* buffer) {
    double localMinutes = minutesFromMidnight + (timeZoneOffset * 60.0);
    
    if (localMinutes < 0) localMinutes += 1440.0;
    if (localMinutes >= 1440) localMinutes -= 1440.0;
    
    int hours = (int)(localMinutes / 60.0);
    int minutes = (int)(localMinutes - (hours * 60));
    
    sprintf(buffer, "%02d:%02d", hours, minutes);
}

SolarTimes calculateSolarTimes(int dayOfYear, double latitude, double longitude) {
    SolarTimes result = {0.0, 0.0, 1};
    
    double gamma = (2.0 * M_PI / 365.0) * (dayOfYear - 1);
    
    double eqtime = 229.18 * (0.000075 + 0.001868 * cos(gamma) - 0.032077 * sin(gamma) 
                    - 0.014615 * cos(2.0 * gamma) - 0.040849 * sin(2.0 * gamma));
    
    double decl = 0.006918 - 0.399912 * cos(gamma) + 0.070257 * sin(gamma) 
                  - 0.006758 * cos(2.0 * gamma) + 0.000907 * sin(2.0 * gamma) 
                  - 0.002697 * cos(3.0 * gamma) + 0.00148 * sin(3.0 * gamma);
    
    double zenithRad = degToRad(90.833); 
    double latRad = degToRad(latitude);
    
    double cosHA = (cos(zenithRad) / (cos(latRad) * cos(decl))) - (tan(latRad) * tan(decl));
    
    if (cosHA > 1.0 || cosHA < -1.0) {
        result.success = 0;
        return result;
    }
    
    double haRad = acos(cosHA);
    double haDeg = radToDeg(haRad);
    
    result.sunrise = 720.0 - (4.0 * (longitude + haDeg)) - eqtime;
    result.sunset = 720.0 - (4.0 * (longitude - haDeg)) - eqtime;
    
    return result;
}

int sunrise_test() 
{
    // Inputs
    const char* inputDate = "2026-07-17"; 
    double latitude = 29.7604;
    double longitude = -95.3698; 
    double timeZoneOffset = -5.0; // CDT (UTC -5)
    
    int validDate = 0;
    int dayOfYear = calculateDayOfYear(inputDate, &validDate);
    
    if (!validDate) {
        printf("Error: Invalid date format or values provided. Use YYYY-MM-DD.\n");
        return 1;
    }
    
    SolarTimes times = calculateSolarTimes(dayOfYear, latitude, longitude);
    
    if (times.success) {
        char sunriseStr[6];
        char sunsetStr[6];
        
        formatTime(times.sunrise, timeZoneOffset, sunriseStr);
        formatTime(times.sunset, timeZoneOffset, sunsetStr);
        
        printf("Input Date: %s (Day of Year: %d)\n", inputDate, dayOfYear);
        printf("Latitude: %.4f, Longitude: %.4f\n", latitude, longitude);
        printf("Sunrise: %s\n", sunriseStr);
        printf("Sunset:  %s\n", sunsetStr);
    } else {
        printf("The location experiences polar day or polar night on this date.\n");
    }
    
    return 0;
}

int get_sunrise_mod(void) 
{
   // Inputs
   char inputDate[32]; 
   int validDate = 0;
   int dayOfYear;
   SolarTimes times;
   int mod = 0;
   
   get_date_string_from_unix_time(unix_time, inputDate, sizeof(inputDate), 1, 1);   

   dayOfYear = calculateDayOfYear(inputDate, &validDate);

   if (!validDate) 
   {
      printf("get_sunrise_mod: Error: Invalid date format or values provided. Use YYYY-MM-DD.\n");
      return 1;
   }
   
   times = calculateSolarTimes(dayOfYear, sys->latitude, sys->longitude);
   
   if (times.success) 
   {
      mod = (int)(times.sunrise + sys->timezone_offset);
   } 
   else 
   {
      printf("get_sunrise_mod: The location experiences polar day or polar night on this date.\n");
   }
      
   
   return(mod);
}

int get_sunset_mod(void) 
{
   // Inputs
   char inputDate[32]; 
   int validDate = 0;
   int dayOfYear;
   SolarTimes times;
   int mod = 0;
   
   get_date_string_from_unix_time(unix_time, inputDate, sizeof(inputDate), 1, 1);   

   dayOfYear = calculateDayOfYear(inputDate, &validDate);

   if (!validDate) 
   {
      printf("get_sunset_mod: Error: Invalid date format or values provided. Use YYYY-MM-DD.\n");
      return 1;
   }
   
   times = calculateSolarTimes(dayOfYear, sys->latitude, sys->longitude);
   
   if (times.success) 
   {
      mod = (int)(times.sunset + sys->timezone_offset);
   } 
   else 
   {
      printf("get_sunset_mod: The location experiences polar day or polar night on this date.\n");
   }
      
   
   return(mod);
}

