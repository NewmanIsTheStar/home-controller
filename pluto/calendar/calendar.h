/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// ACRONYMS
// MOD = Minute of Day  :: range 0 to 1439 where 0 = midnight
// MOW = Minute of Week :: range 0 to 10079 where 0 = midnight on the first day of the week (user configurable as either Sunday or Monday)

#ifndef CALENDAR_H
#define CALENDAR_H

#define MINUTES_IN_WEEK (10080)
#define MINUTES_IN_DAY (1440)
#define MINUTES_IN_HOUR (60)
#define HOURS_IN_DAY (24)
#define DAYS_IN_WEEK (7)

typedef enum
{
    SCHEDULE_FUTURE = 0,
    SCHEDULE_NOW = 1,
    SCHEDULE_NEVER = 2,
} SCHEDULE_QUERY_STATUS_LT;

typedef struct {
    double sunrise; 
    double sunset;  
    int success;    
} SolarTimes;

// calendar_sun.c
SolarTimes calculateSolarTimes(int dayOfYear, double latitude, double longitude);
int get_sunrise_mod(void);
int get_sunset_mod(void);

// calendar_daylight_saving.c
int set_daylight_saving_dates(void);
int sanitize_daylight_saving_date(char *in, char *out, int len);
int daylight_savings_active(datetime_t date);
int get_daylight_saving_month_and_day(int year, char *date_description, int *daylight_savings_month, int *daylight_savings_day);
const char *day_name(int day);

// calendar_webui.c
int set_calendar_html_page(void);
int calendar_timeserver_failsafe(void);

// calendar_sntp.c
bool sntp_alive(void);
#ifdef FAKE_RTC
uint32_t rtc_update(void);
int8_t rtc_get_datetime(datetime_t *date);
int8_t rtc_set_datetime(uint32_t sec);
int8_t get_datetime(datetime_t *date, int localtime);
#endif
int8_t get_real_time_clock_seconds(void);

// calendar_numric.c
int get_day_of_week(int m,int d,int y);
int get_dow_and_mod_local_tz(int *dow, int *mod);
bool mow_between(int time_mow, int start_mow, int end_mow);
int mow_future_delta(int start_mow, int end_mow);
int get_day_from_mow(int mow);
int get_mow_local_tz(int *mow);
int8_t get_datetime_from_unix_time(uint32_t unixtime, datetime_t *date, int *effective_offset, int localtime);

// calendar_string.c
int get_timestamp(char *timestamp, int len, int isoformat, int localtime);
int get_local_time_string(char *time_string, int len);
int get_local_date_string(char *date_string, int len);
int get_local_day_string(char *day_string, int len);
int get_timestamp_from_unix_time(uint32_t unixtime, char *timestamp, int len, int isoformat, int localtime);
int get_local_timestamp_from_unix_time(uint32_t unixtime, char *timestamp, int len);
int get_time_string_from_unix_time(uint32_t unixtime, char *time_string, int len, int isoformat, int localtime);
int get_date_string_from_unix_time(uint32_t unixtime, char *date_string, int len, int isoformat, int localtime);
int get_day_string_from_unix_time(uint32_t unixtime, char *day_string, int len, int isoformat, int localtime);
int mow_to_time_string(char *string, int length, int mow);
int time_string_to_mow(char *string, int length, int day);
int get_delta_string_from_delta_seconds(char *string, int len, uint32_t delta_seconds);

// TODO: move to application specific code for sprinkler controller (usurper)
SCHEDULE_QUERY_STATUS_LT get_next_irrigation_period(int *start_mow, int *end_mow, int *delay_mins, int *zone);



#endif
