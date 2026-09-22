/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 // be cautious what you include here lwip and fcntl have some important conflicts related to BSD / sockets
#include <stdio.h>
#include <stdlib.h>
//#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <lwip/arch.h>
#include "picofs.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"


#include <sys/stat.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "config.h"
#include "pluto.h"
#include "utility.h"


#include "picofs.h"
#include "system_config.h"
#include "config.h"

//#define DISABLE_CONFIG_UPGRADE

SYSTEM_CONFIG_T *sys = NULL;


// system configuration conversion functions
void syscfg_blank_to_v1(void *previous_config);

// table of conversion functions -- these are run sequentially from the starting version to convert to the latest version
CONFIG_CONVERSION_T syscfg_info[] =
{
    {1,      sizeof(SYSTEM_CONFIG_T),   offsetof(SYSTEM_CONFIG_T, version),   offsetof(SYSTEM_CONFIG_T, crc),   &syscfg_blank_to_v1},                 
};

int syscfg_info_rows = NUM_ROWS(syscfg_info);



/*!
 * \brief Set default values for configuration v1
 * 
 * \return 0 on success, -1 on error
 */
void syscfg_blank_to_v1(void *previous_config)
{
    int i;

    printf("Initializing configuration version 1\n");

    // version
    sys->version = 1;

    // personality
    sys->personality = HOME_CONTROLLER;

    // network
    STRNCPY(sys->wifi_country, "World Wide", sizeof(sys->wifi_country));      
    sys->wifi_ssid[0] = 0;
    sys->wifi_password[0] = 0;
    sys->dhcp_enable = 1;
    STRNCPY(sys->host_name, APP_NAME, sizeof(sys->host_name));
    sys->ip_address[0] = 0;
    sys->network_mask[0] = 0;
    
    // time
    sys->timezone_offset = -6*60;
    sys->daylightsaving_enable = 1;  
    STRNCPY(sys->daylightsaving_start, "Second Sunday in March", sizeof(sys->daylightsaving_start));
    STRNCPY(sys->daylightsaving_end, "First Sunday in November", sizeof(sys->daylightsaving_end));
    STRNCPY(sys->time_server[0], "pool.ntp.org", sizeof(sys->time_server[0]));
    STRNCPY(sys->time_server[1], "time.google.com", sizeof(sys->time_server[1]));
    STRNCPY(sys->time_server[2], "time.facebook.com", sizeof(sys->time_server[2]));
    STRNCPY(sys->time_server[3], "time.windows.com", sizeof(sys->time_server[3]));        

    // syslog
    STRNCPY(sys->syslog_server_ip, "spud.badnet", sizeof(sys->syslog_server_ip));         
    sys->syslog_enable = 0;
    
    // foibles
    sys->use_archaic_units = 1;
    sys->use_simplified_english = 1;
    sys->use_monday_as_week_start = 0;

    // gpio
    for(i=0; i<NUM_ROWS(sys->gpio_default); i++)
    {
        sys->gpio_default[i] = GP_UNINITIALIZED;
    } 
    
    // mqtt
    sys->mqtt_user[0] = 0;
    sys->mqtt_password[0] = 0;
    sys->mqtt_broker_address[0] = 0;

    // geolocation
    sys->latitude = 29.7604;
    sys->longitude = -95.3698; 

}



