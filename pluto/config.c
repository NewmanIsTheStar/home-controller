/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/sockets.h"


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "config.h"
#include "pluto.h"
#include "utility.h"

#include "flash.h"

//#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
//#define DISABLE_CONFIG_VALIDATION (1)
//#define DISABLE_CONFIG_UPGRADE (1)
//#define DISABLE_CONFIG_WRITE [1]

bool config_compare_flash_ram(bool stop_at_first_difference);
int config_validate(void);
void config_system_variable_initialize(void);
void config_blank_to_v1(void *previous_config);


NON_VOL_VARIABLES_T config;
static int config_dirty_flag = 0;
static NON_VOL_CONVERSION_T config_info[] =
{
    {1,      offsetof(NON_VOL_VARIABLES_T, version),   offsetof(NON_VOL_VARIABLES_T, crc),   &config_blank_to_v1},                 
};


/*!
 * \brief Set default values for configuration v1
 * 
 * \return 0 on success, -1 on error
 */
void config_blank_to_v1(void *previous_config)
{
    int i;

    printf("Initializing configuration version 1\n");

    // version
    config.version = 1;

    // personality
    config.personality = HOME_CONTROLLER;
    
    // home controller
    config.hc_enable = 1;

    for(i=0; i<NUM_ROWS(config.shelly_device_ip); i++)
    {
        config.shelly_device_ip[i][0] = 0;
        config.shelly_device_ip[i][1] = 0;
        config.shelly_device_ip[i][2] = 0;
        config.shelly_device_ip[i][3] = 0;                        
        config.shelly_device_type[i] = 0;        
    }

    for(i=0; i<NUM_ROWS(config.shelly_parameter_device_index); i++)
    {    
        config.shelly_parameter_device_index[i] = 255;
        config.shelly_parameter_name_index[i] = 255;
    }

    for(i=0; i<NUM_ROWS(config.shelly_parameter_value); i++)
    {     
        config.shelly_parameter_value[i][0] = 0;   
        config.shelly_parameter_name[i][0] = 0;        
    }
    
}


// ************************************************************************************************************************
// ************************************************************************************************************************

/*!
 * \brief Record that configuration copy in RAM was altered and may now differ from the flash copy
 */
void config_changed(void)
{
    config_dirty_flag = 1;
}

/*!
 * \brief Check if RAM copy of configuration differs from flash copy.  Optionally clear the dirty flag.
 * 
 * \param[in]    clear_flag Set the dirty flag to false after returning its value
 * 
 * \return true if config in RAM differs from config in flash, otherwise flase
 */
bool config_dirty(bool clear_flag)
{
    int dirty = false;

    if (config_dirty_flag)
    {
        dirty = true;

        if (clear_flag)
        {
            config_dirty_flag = 0;
        }
    }

    return (dirty);
}

/*!
 * \brief Copy the configuration from flash into RAM.  Set default values if flash is corrupt.
 * 
 * \return 0 on success, -1 on error
 */
int config_read(void)
{
    int err = 0;

    // read configuration from flash
    flash_read_non_volatile_variables(CONFIG_STANDARD);

#ifdef DISABLE_CONFIG_VALIDATION
    printf("Configuration validation disabled!  Using whatever random garbage happens to be in flash...\n");
#else
    // check and correct configuration
    config_validate();
#endif

    return(err);
}

/*!
 * \brief Copy the configuration from RAM into flash if they differ.
 * 
 * \return 0 on success, -1 on error
 */
int config_write(void)
{
    int err = 0;

    #ifdef DISABLE_CONFIG_WRITE
    printf("Configuration Writes are disabled!\n");
    #else
    // write configuration to flash if altered recently
    if (config_dirty(true))
    {
        // wait for 5 second period with no config changes
        do 
        {
            SLEEP_MS(5000);
        } while (config_dirty(true));

        // update crc
        config.system_crc = crc_buffer((uint8_t *)&config, offsetof(NON_VOL_VARIABLES_T, system_crc));         
        config.crc = crc_buffer((uint8_t *)&config, offsetof(NON_VOL_VARIABLES_T, crc)); 
         
        // compare ram and flash copies
        if (memcmp((char *)(XIP_BASE +  FLASH_TARGET_OFFSET), ((char *)&config), sizeof(config)))
        {
            printf("Writing configuration to flash\n");

            if (err = flash_write_non_volatile_variables())
            {
                printf("Failed to write configuraiton to flash (%d)\n", err);                
            } 
            else if (config_compare_flash_ram(false))
            {
                flash_dump_config(CONFIG_STANDARD);
            }          
        }           
        else
        {
            printf("Refusing to write configuration to flash as RAM and flash copies are identical\n");
        }

        // check for collision
        if (config.crc != crc_buffer((uint8_t *)&config, offsetof(NON_VOL_VARIABLES_T, crc)))
        {
            // config was updated by another task after we computed the crc and possibly before we wrote to flash
            printf("Config update occured while writing to flash, will retry\n");
            
            config_changed();

            err = -1;
        }          
    }  
    #endif

    return(err);
}

/*!
 * \brief Compare flash and RAM copies of configuration
 * 
 * \return 0 = no difference, 1 = difference
 */
bool config_compare_flash_ram(bool stop_at_first_difference)
{
    NON_VOL_VARIABLES_T *non_vol;
    int i;
    int len;
    uint16_t ram_crc;
    uint16_t flash_crc;    
    bool difference_found = false;

    for (i=0; i<sizeof(config); i++)
    {
        if (((char *)(XIP_BASE +  FLASH_TARGET_OFFSET))[i] != ((char *)&config)[i])
        {
            if (!difference_found)
            {
                // printf headings
                printf("     offset\tflash\tram\n");
            }

            // print difference
            printf("%08x:\t%02x \t%02x\n", i, ((char *)(XIP_BASE +  FLASH_TARGET_OFFSET))[i], ((char *)&config)[i]);
            
            difference_found = true;

            if (stop_at_first_difference)
            {
                break;
            }
        }
    }
    
    return(difference_found);
}

/*!
 * \brief Check configuration is valid and upgrade if necessary 
 * 
 * \return 0 on success, -1 on error
 */
int config_validate(void)
{
    int err = 0;
    int i = 0;
    int version_from_flash = 0;
    uint16_t crc_from_flash = 0;
    uint16_t calculated_crc = 0;
    int latest_valid_config_version = 0;
    void *previous_config = NULL;
    CONFIG_TYPE_T config_type;

    for(config_type=CONFIG_STANDARD; config_type < NUM_CONFIG_TYPES; config_type++)
    {

        // read configuration into RAM
        flash_read_non_volatile_variables(config_type); 

        // check for valid configuration
        for(i=0; i < NUM_ROWS(config_info); i++)
        {
            version_from_flash = *((int *)((uint8_t *)&config + config_info[i].version_offset));
            crc_from_flash = *((uint16_t *)((uint8_t *)&config + config_info[i].crc_offset));
            calculated_crc = crc_buffer((uint8_t *)&config, config_info[i].crc_offset);        

            if ((version_from_flash == config_info[i].version) && (crc_from_flash == calculated_crc))
            {
                printf("Found valid configuration version %d\n", version_from_flash);
                latest_valid_config_version = version_from_flash;
            }
        }

        // check if we found a valid config version
        if (latest_valid_config_version != 0)        
        {
            // we found a valid config so stop searching
            break;
        }
        else
        {
            // no valid config so try to fallback to system config only
            crc_from_flash = *((uint16_t *)((uint8_t *)&config + offsetof(NON_VOL_VARIABLES_T, system_crc)));
            calculated_crc = crc_buffer((uint8_t *)&config, offsetof(NON_VOL_VARIABLES_T, system_crc));

            if(crc_from_flash == calculated_crc)
            {
                printf("Found valid system configuration variables (e.g. network config).  These will be preserved.\n");
                break;
            }
            else
            {
                config_system_variable_initialize();
            }
        }

    }

#ifndef DISABLE_CONFIG_UPGRADE

    // obtain pointer to previous config if available
    if (version_from_flash > 0)
    {
        previous_config = flash_get_config_location(config_type);
    }

    // upgrade configuration sequentially to latest version 
    for(i=0; i < NUM_ROWS(config_info); i++)
    {
        if (latest_valid_config_version < config_info[i].version)
        {
            config_info[i].upgrade_function(previous_config);
        }
    }
#else
    if (latest_valid_config_version < config_info[i].version)
    {
        for (;;)
        {
            printf("BAD CONFIG!\n");
            flash_dump();

            SLEEP_MS(10000);
        }
    }
#endif

    return(err);
}


/*!
 * \brief Set a default time server in config if all four time server entries are blank
 * 
 * \return 0 on success, -1 on error
 */
int config_timeserver_failsafe(void)
{
    // failsafe - if no timeserver configured try pool.ntp.org
    if ((config.time_server[0][0] == 0) &&
        (config.time_server[1][0] == 0) &&
        (config.time_server[2][0] == 0) &&
        (config.time_server[3][0] == 0))
    {
        STRNCPY(config.time_server[0], "pool.ntp.org", sizeof(config.time_server[0]));
    }

    return(0);
}

/*!
 * \brief Set default values for system variables
 * 
 * \return 0 on success, -1 on error
 */
void config_system_variable_initialize(void)
{
    int i;

    printf("Initializing configuration system variables in RAM\n");

    // personality
    config.personality = NO_PERSONALITY;

    // network
    STRNCPY(config.wifi_country, "World Wide", sizeof(config.wifi_country));      
    config.wifi_ssid[0] = 0;
    config.wifi_password[0] = 0;
    config.dhcp_enable = 1;
    STRNCPY(config.host_name, APP_NAME, sizeof(config.host_name));
    config.ip_address[0] = 0;
    config.network_mask[0] = 0;
    
    // time
    config.timezone_offset = -6*60;
    config.daylightsaving_enable = 1;  
    STRNCPY(config.daylightsaving_start, "Second Sunday in March", sizeof(config.daylightsaving_start));
    STRNCPY(config.daylightsaving_end, "First Sunday in November", sizeof(config.daylightsaving_end));
    STRNCPY(config.time_server[0], "pool.ntp.org", sizeof(config.time_server[0]));
    STRNCPY(config.time_server[1], "time.google.com", sizeof(config.time_server[1]));
    STRNCPY(config.time_server[2], "time.facebook.com", sizeof(config.time_server[2]));
    STRNCPY(config.time_server[3], "time.windows.com", sizeof(config.time_server[3]));        

    // syslog
    STRNCPY(config.syslog_server_ip, "spud.badnet", sizeof(config.syslog_server_ip));         
    config.syslog_enable = 0;
    
    // foibles
    config.use_archaic_units = 1;
    config.use_simplified_english = 1;
    config.use_monday_as_week_start = 0;

    // gpio
    for(i=0; i<NUM_ROWS(config.gpio_default); i++)
    {
        config.gpio_default[i] = GP_UNINITIALIZED;
    } 
    
    // mqtt
    config.mqtt_user[0] = 0;
    config.mqtt_password[0] = 0;
    config.mqtt_broker_address[0] = 0;
}
