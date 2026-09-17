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


#include <sys/stat.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "syscfg.h"
#include "pluto.h"
#include "utility.h"

#include "flash.h"
#include "picofs.h"

//#define DISABLE_SYSCFG_UPGRADE

// system configuration conversion functions
void syscfg_blank_to_v1(void *previous_config);

// table of conversion functions -- these are run sequentially from the starting version to convert to the latest version
static SYSTEM_CONVERSION_T syscfg_info[] =
{
    {1,      offsetof(SYSTEM_CONFIG_T, version),   offsetof(SYSTEM_CONFIG_T, crc),   &syscfg_blank_to_v1},                 
};

/*!
 * \brief Check configuration is valid and upgrade if necessary 
 * 
 * \return 0 on success, -1 on error
 */
int syscfg_validate(void)
{
    int err = 0;
    int i = 0;
    int version_from_flash = 0;
    uint16_t crc_from_flash = 0;
    uint16_t calculated_crc = 0;
    int latest_valid_syscfg_version = 0;
    void *previous_config = NULL;
    CONFIG_TYPE_T syscfg_type;


    // read configuration into RAM
    err = syscfg_map_file(); 

    if (!err)
    {
        // check for valid configuration
        for(i=0; i < NUM_ROWS(syscfg_info); i++)
        {
            version_from_flash = *((int *)((uint8_t *)sys + syscfg_info[i].version_offset));
            crc_from_flash = *((uint16_t *)((uint8_t *)sys + syscfg_info[i].crc_offset));
            calculated_crc = crc_buffer((uint8_t *)sys, syscfg_info[i].crc_offset);        

            if ((version_from_flash == syscfg_info[i].version) && (crc_from_flash == calculated_crc))
            {
                printf("Found valid system system configuration version %d\n", version_from_flash);
                latest_valid_syscfg_version = version_from_flash;
                break;
            }
        }
    }
    

#ifndef DISABLE_SYSCFG_UPGRADE
    // obtain pointer to previous config if available
    if (version_from_flash > 0)
    {
        previous_config = syscfg_get_flash_location();  //TODO: should have this pointer from the file open
    }

    // upgrade configuration sequentially to latest version 
    for(i=0; i < NUM_ROWS(syscfg_info); i++)
    {
        if (latest_valid_syscfg_version < syscfg_info[i].version)
        {
            syscfg_info[i].upgrade_function(previous_config);
        }
    }
#else
    if (latest_valid_syscfg_version < syscfg_info[i].version)
    {
        for (;;)
        {
            printf("BAD CONFIG!\n");
            hex_dump(sys, sizeof(SYSTEM_VARIABLES_T));

            SLEEP_MS(10000);
        }
    }
#endif

    return(err);
}

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



