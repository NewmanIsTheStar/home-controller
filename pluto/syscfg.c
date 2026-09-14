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


//#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
//#define DISABLE_SYSCFG_VALIDATION (1)
//#define DISABLE_SYSCFG_UPGRADE (1)
//#define DISABLE_SYSCFG_WRITE [1]

int syscfg_map_file(void);
int syscfg_mmap(char *filename);
int syscfg_sync_file(void);
void *syscfg_get_flash_location(void);
bool syscfg_compare_flash_ram(bool stop_at_first_difference, bool print_differences);
int syscfg_validate(void);
void syscfg_system_variable_initialize(void);
void syscfg_blank_to_v1(void *previous_config);

int syscfg_fd = -1;
SYSTEM_VARIABLES_T *sys;
SYSTEM_VARIABLES_T system_config;
static int syscfg_dirty_flag = 0;
static SYSTEM_CONVERSION_T syscfg_info[] =
{
    {1,      offsetof(SYSTEM_VARIABLES_T, version),   offsetof(SYSTEM_VARIABLES_T, crc),   &syscfg_blank_to_v1},                 
};


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

    syscfg_system_variable_initialize();

}


// ************************************************************************************************************************
// ************************************************************************************************************************

/*!
 * \brief Record that configuration copy in RAM was altered and may now differ from the flash copy
 */
void syscfg_changed(void)
{
    syscfg_dirty_flag = 1;
}

/*!
 * \brief Check if RAM copy of configuration differs from flash copy.  Optionally clear the dirty flag.
 * 
 * \param[in]    clear_flag Set the dirty flag to false after returning its value
 * 
 * \return true if config in RAM differs from config in flash, otherwise flase
 */
bool syscfg_dirty(bool clear_flag)
{
    int dirty = false;

    if (syscfg_dirty_flag)
    {
        dirty = true;

        if (clear_flag)
        {
            syscfg_dirty_flag = 0;
        }
    }

    return (dirty);
}

/*!
 * \brief Copy the configuration from flash into RAM.  Set default values if flash is corrupt.
 * 
 * \return 0 on success, -1 on error
 */
int syscfg_read(void)
{
    int err = 0;

#ifdef DISABLE_SYSCFG_VALIDATION
    // read configuration from flash
    err = syscfg_map_file();  
    printf("Warning: Configuration validation disabled. Using whatever random garbage happens to be in flash...\n");    
#else
    // check and correct configuration
    err = syscfg_validate();
#endif

    return(err);
}

/*!
 * \brief Copy the configuration from RAM into flash if they differ.
 * 
 * \return 0 on success, -1 on error
 */
int syscfg_write(void)
{
    int err = 0;


    #ifdef DISABLE_SYSCFG_WRITE
    printf("Configuration Writes are disabled!\n");
    #else
    // write configuration to flash if altered recently
    if (syscfg_dirty(true))
    {
        // wait for 5 second period with no config changes
        do 
        {
            SLEEP_MS(5000);
        } while (syscfg_dirty(true));

        // update crc        
        sys->crc = crc_buffer((uint8_t *)sys, offsetof(SYSTEM_VARIABLES_T, crc)); 
         
        // compare ram and flash copies
        if (syscfg_compare_flash_ram(false, false))
        {
            printf("Writing configuration to flash\n");

            if (err = syscfg_sync_file())
            {
                printf("Failed to write system configuration to flash (%d)\n", err);                
            } 
            else if (syscfg_compare_flash_ram(false, true))  // we just wrote the config so there shoud now be no differences
            {
                printf("syscfg_write: DUMPING SYSTEM CONFIG because difference found after writing to flash!\n");
                hex_dump((const uint8_t *)sys, sizeof(SYSTEM_VARIABLES_T));
            }          
        }           
        else
        {
            printf("Refusing to write system configuration to flash as RAM and flash copies are identical\n");
        }

        // check for collision
        if (sys->crc != crc_buffer((uint8_t *)sys, offsetof(SYSTEM_VARIABLES_T, crc)))
        {
            // config was updated by another task after we computed the crc and possibly before we wrote to flash
            printf("System configuration update occured while writing to flash, will retry\n");
            
            syscfg_changed();

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
bool syscfg_compare_flash_ram(bool stop_at_first_difference, bool print_differences)
{
    int i;
    bool difference_found = false;
    char *syscfg_location_in_flash = NULL;

    syscfg_location_in_flash = syscfg_get_flash_location();

    if (syscfg_location_in_flash)
    {
        if (print_differences)
        {
            for (i=0; i<sizeof(SYSTEM_VARIABLES_T); i++)
            {
                if (syscfg_location_in_flash[i] != ((char *)sys)[i])
                {
                    if (!difference_found)
                    {
                        // printf headings
                        printf("     offset\tflash\tram\n");
                    }

                    // print difference
                    printf("%08x:\t%02x \t%02x\n", i, syscfg_location_in_flash[i], ((char *)sys)[i]);
                    
                    difference_found = true;

                    if (stop_at_first_difference)
                    {
                        break;
                    }
                }
            }
        }
        else
        {
            if (memcmp(syscfg_location_in_flash, ((char *)sys), sizeof(SYSTEM_VARIABLES_T)))
            {
                printf("syscfg_compare_flash_ram: memcmp() found difference.  flash location = %p\n", syscfg_location_in_flash);
                difference_found = true;
            }
        }
    }
    else
    {
        printf("syscfg_compare_flash_ram: DEFAULTING TO DIFFERENCE FOUND\n");
        difference_found = true;
    }
    
    return(difference_found);
}

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
            }
        }

        // // check if we found a valid config version
        // if (latest_valid_syscfg_version != 0)        
        // {
        //     // we found a valid config so stop searching
        //     break;
        // }
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
 * \brief Set a default time server in config if all four time server entries are blank
 * 
 * \return 0 on success, -1 on error
 */
int syscfg_timeserver_failsafe(void)
{
    // failsafe - if no timeserver configured try pool.ntp.org
    if ((sys->time_server[0][0] == 0) &&
        (sys->time_server[1][0] == 0) &&
        (sys->time_server[2][0] == 0) &&
        (sys->time_server[3][0] == 0))
    {
        STRNCPY(sys->time_server[0], "pool.ntp.org", sizeof(sys->time_server[0]));
    }

    return(0);
}

/*!
 * \brief Set default values for system variables
 * 
 * \return 0 on success, -1 on error
 */
void syscfg_system_variable_initialize(void)
{
    int i;

    printf("Initializing system configuration variables in RAM\n");

    // personality
    sys->personality = NO_PERSONALITY;

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


/*!
 * \brief Copy configuration from flash to RAM
 * 
 * \return 0 on success, -1 on error
 */
int syscfg_map_file(void)
{
    int err = 0;
    struct stat file_status;

    // map file into RAM, create the file if it doesn't already exist
    err = syscfg_mmap("system.cfg");
    if (err)
    {
        printf("syscfg_map_file: failed to mmap file: system.cfg\n");
    }          
    
    return(err);
}

/*!
 * \brief Copy configuration from RAM into flash
 * 
 * \return 0 on success
 */
int syscfg_sync_file(void)
{
    int err = 0;

    err = picofs_msync(sys, sizeof(SYSTEM_VARIABLES_T), MS_SYNC);

    return(err);
}

void *syscfg_get_flash_location(void)
{
    void *location = NULL;
    FILE_TRAILER_T *config_trailer = NULL;


    if (!picofs_find_file("system.cfg", FS_INVALID_FID, &config_trailer))
    {
        location = (char *)config_trailer + sizeof(FILE_TRAILER_T) - config_trailer->file_size;
    }

    printf("syscfg_get_flash_location: returning system configuration location = %p\n", location);

    return(location);
}


/*!
 * \brief map configuration file into memory for random access
 *
 * \param filename file containing configuration
 * 
 * \return nothing
 */
int syscfg_mmap(char *filename) 
{
    size_t FILE_SIZE = 4096; // 4 KB (typically matches 1 memory page)
    char *map;

    // open the file with for read/write (create if it doesn't exist)
    syscfg_fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (syscfg_fd == -1) 
    {
        perror("syscfg_mmap: Error opening/creating file");
        return EXIT_FAILURE;
    }
    //printf("config_mmap: config_fd = %d\n", config_fd);

    // adjust the file length to match the current configuration version TODO: make this the largest known config to support conversion to a smaller config
    if (ftruncate(syscfg_fd, sizeof(SYSTEM_VARIABLES_T)) == -1) 
    {
        perror("syscfg_mmap: Error setting file size");
        close(syscfg_fd);
        syscfg_fd = -1;
        return EXIT_FAILURE;
    }

    // map the file into the process address space
    map = picofs_mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, syscfg_fd, 0);
    if (map == MAP_FAILED) 
    {
        perror("syscfg_mmap: Error mapping the file");
        close(syscfg_fd);
        syscfg_fd = -1;
        return EXIT_FAILURE;
    }
    //printf("syscfg_mmap: @%p\n", map);
    
    // point cfg at the mapped file
    sys = (SYSTEM_VARIABLES_T *)map;

    return EXIT_SUCCESS;
}