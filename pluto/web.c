/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "pico/cyw43_arch.h"
#include "calendar.h"
#include "pluto.h"
#include "web.h"

// global variables
WEB_VARIABLES_T web;

/*!
 * \brief Initialize web interface variables
 *
 * \return 0 on success
 */
int init_web_variables(void)
{
    // zero web structure
    memset(&web, 0, sizeof(web));

    cyw43_hal_get_mac(CYW43_ITF_STA, web.mac);
    printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]);

    web.access_point_mode = 0;
    
    // web.outside_temperature = 0;
    // web.wind_speed = 0;
    // web.daily_rain = 0;
    // web.weekly_rain = 0;
    // web.trailing_seven_days_rain = 0;
    // web.us_last_rx_packet = 0;  
    // web.soil_moisture[0] = 0; 

    // web.irrigation_test_enable = 0; 

    // STRNCPY(web.last_usurped_timestring,"never", sizeof(web.last_usurped_timestring));
    // STRNCPY(web.last_completed_timestring,"never", sizeof(web.last_completed_timestring));    
    // STRNCPY(web.watchdog_timestring,"never", sizeof(web.watchdog_timestring));

    web.status_message[0] = 0;
    web.stack_message[0] = 0;

    web.socket_max = 0;
    web.bind_failures = 0;
    web.connect_failures = 0;  
    web.syslog_transmit_failures = 0;
    //web.govee_transmit_failures = 0;
    // web.weather_station_transmit_failures = 0;
    web.bind_failures = 0;
    web.connect_failures = 0;   

    // web.led_current_pattern = 0;
    // web.led_current_transition_delay = 0;
    // web.led_last_request_ip[0] = 0;

    STRNCPY(web.software_server,"psycho.badnet", sizeof(web.software_server));
    STRNCPY(web.software_url,"http://psycho.badnet/hc", sizeof(web.software_url));
    STRNCPY(web.software_file,"/pluto.bin", sizeof(web.software_file));        
    web.download_now = 0;

    // set default web page
    // set_calendar_html_page();


    return(0);
}