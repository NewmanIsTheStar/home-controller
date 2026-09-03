/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

// #include "lwip/sockets.h"


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

// #define _GNU_SOURCE // Required for O_DIRECT on Linux
//#include <fcntl.h>
//#include <unistd.h>
//#include <stdlib.h>
//#include <stdio.h>


#include "config.h"
#include "pluto.h"
#include "utility.h"

#include "flash.h"
#include "picofs.h"

// for mmap test
// #include <stdio.h>
// #include <stdlib.h>
//#include <sys/mman.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <string.h>

//#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
//#define DISABLE_CONFIG_VALIDATION (1)
//#define DISABLE_CONFIG_UPGRADE (1)
//#define DISABLE_CONFIG_WRITE [1]

bool config_compare_flash_ram(CONFIG_TYPE_T config_type, bool stop_at_first_difference, bool print_differences);
int config_validate(void);
void config_system_variable_initialize(void);
void config_blank_to_v1(void *previous_config);

NON_VOL_VARIABLES_T *cfg;
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
    cfg->version = 1;

    // personality
    cfg->personality = HOME_CONTROLLER;
    
    // home controller
    cfg->hc_enable = 1;

    // for(i=0; i<NUM_ROWS(cfg->shelly_device_ip); i++)
    // {
    //     cfg->shelly_device_ip[i][0] = 0;
    //     cfg->shelly_device_ip[i][1] = 0;
    //     cfg->shelly_device_ip[i][2] = 0;
    //     cfg->shelly_device_ip[i][3] = 0;                        
    //     cfg->shelly_device_type[i] = 0;        
    // }

    // for(i=0; i<NUM_ROWS(cfg->shelly_parameter_device_index); i++)
    // {    
    //     cfg->shelly_parameter_device_index[i] = 255;
    //     cfg->shelly_parameter_name_index[i] = 255;
    // }

    // for(i=0; i<NUM_ROWS(cfg->shelly_parameter_value); i++)
    // {     
    //     cfg->shelly_parameter_value[i][0] = 0;   
    //     cfg->shelly_parameter_name[i][0] = 0;        
    // }
    
    for(i=0; i<NUM_ROWS(cfg->automation_name); i++)
    {
        //cfg->automation_name[i][0] = 0;
        sprintf(cfg->automation_name[i], "automation%02d", i);
        cfg->automation_triggered[i] = 0;
        cfg->automation_state[i] = 0;
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
int config_read(CONFIG_TYPE_T config_type)
{
    int err = 0;

    // set config pointer
    //cfg = &config;

#ifdef DISABLE_CONFIG_VALIDATION
    // read configuration from flash
    err = flash_read_non_volatile_variables(config_type);    
    printf("Warning: Configuration validation disabled. Using whatever random garbage happens to be in flash...\n");    
#else
    // check and correct configuration
    err = config_validate();
#endif

    return(err);
}

/*!
 * \brief Copy the configuration from RAM into flash if they differ.
 * 
 * \return 0 on success, -1 on error
 */
int config_write(CONFIG_TYPE_T config_type)
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
        cfg->system_crc = crc_buffer((uint8_t *)cfg, offsetof(NON_VOL_VARIABLES_T, system_crc));         
        cfg->crc = crc_buffer((uint8_t *)cfg, offsetof(NON_VOL_VARIABLES_T, crc)); 
         
        // compare ram and flash copies
        if (config_compare_flash_ram(config_type, false, false))
        {
            printf("Writing configuration to flash\n");

            if (err = flash_write_non_volatile_variables(CONFIG_FILE))
            {
                printf("Failed to write configuraiton to flash (%d)\n", err);                
            } 
            else if (config_compare_flash_ram(config_type, false, true))  // we just wrote the config so there shoud now be no differences
            {
                printf("DUMPING CONFIG because difference found after writing to flash!\n");
                flash_dump_config(CONFIG_FILE);
            }          
        }           
        else
        {
            printf("Refusing to write configuration to flash as RAM and flash copies are identical\n");
        }

        // check for collision
        if (cfg->crc != crc_buffer((uint8_t *)cfg, offsetof(NON_VOL_VARIABLES_T, crc)))
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
bool config_compare_flash_ram(CONFIG_TYPE_T config_type, bool stop_at_first_difference, bool print_differences)
{
    int i;
    bool difference_found = false;
    char *config_location_in_flash = NULL;

    config_location_in_flash = flash_get_config_location(config_type);

    if (config_location_in_flash)
    {
        if (print_differences)
        {
            for (i=0; i<sizeof(NON_VOL_VARIABLES_T); i++)
            {
                if (config_location_in_flash[i] != ((char *)cfg)[i])
                {
                    if (!difference_found)
                    {
                        // printf headings
                        printf("     offset\tflash\tram\n");
                    }

                    // print difference
                    printf("%08x:\t%02x \t%02x\n", i, config_location_in_flash[i], ((char *)cfg)[i]);
                    
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
            if (memcmp(config_location_in_flash, ((char *)cfg), sizeof(NON_VOL_VARIABLES_T)))
            {
                printf("config_compare_flash_ram: memcmp() found difference.  flash location = %p\n", config_location_in_flash);
                difference_found = true;
            }
        }
    }
    else
    {
        printf("config_compare_flash_ram: DEFAULTING TO DIFFERENCE FOUND\n");
        difference_found = true;
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

    for(config_type=CONFIG_FILE; config_type < NUM_CONFIG_TYPES; config_type++)
    {

        // read configuration into RAM
        err = flash_read_non_volatile_variables(config_type); 

        if (!err)
        {
            // check for valid configuration
            for(i=0; i < NUM_ROWS(config_info); i++)
            {
                version_from_flash = *((int *)((uint8_t *)cfg + config_info[i].version_offset));
                crc_from_flash = *((uint16_t *)((uint8_t *)cfg + config_info[i].crc_offset));
                calculated_crc = crc_buffer((uint8_t *)cfg, config_info[i].crc_offset);        

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
                crc_from_flash = *((uint16_t *)((uint8_t *)cfg + offsetof(NON_VOL_VARIABLES_T, system_crc)));
                calculated_crc = crc_buffer((uint8_t *)cfg, offsetof(NON_VOL_VARIABLES_T, system_crc));

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
    if ((cfg->time_server[0][0] == 0) &&
        (cfg->time_server[1][0] == 0) &&
        (cfg->time_server[2][0] == 0) &&
        (cfg->time_server[3][0] == 0))
    {
        STRNCPY(cfg->time_server[0], "pool.ntp.org", sizeof(cfg->time_server[0]));
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
    cfg->personality = NO_PERSONALITY;

    // network
    STRNCPY(cfg->wifi_country, "World Wide", sizeof(cfg->wifi_country));      
    cfg->wifi_ssid[0] = 0;
    cfg->wifi_password[0] = 0;
    cfg->dhcp_enable = 1;
    STRNCPY(cfg->host_name, APP_NAME, sizeof(cfg->host_name));
    cfg->ip_address[0] = 0;
    cfg->network_mask[0] = 0;
    
    // time
    cfg->timezone_offset = -6*60;
    cfg->daylightsaving_enable = 1;  
    STRNCPY(cfg->daylightsaving_start, "Second Sunday in March", sizeof(cfg->daylightsaving_start));
    STRNCPY(cfg->daylightsaving_end, "First Sunday in November", sizeof(cfg->daylightsaving_end));
    STRNCPY(cfg->time_server[0], "pool.ntp.org", sizeof(cfg->time_server[0]));
    STRNCPY(cfg->time_server[1], "time.google.com", sizeof(cfg->time_server[1]));
    STRNCPY(cfg->time_server[2], "time.facebook.com", sizeof(cfg->time_server[2]));
    STRNCPY(cfg->time_server[3], "time.windows.com", sizeof(cfg->time_server[3]));        

    // syslog
    STRNCPY(cfg->syslog_server_ip, "spud.badnet", sizeof(cfg->syslog_server_ip));         
    cfg->syslog_enable = 0;
    
    // foibles
    cfg->use_archaic_units = 1;
    cfg->use_simplified_english = 1;
    cfg->use_monday_as_week_start = 0;

    // gpio
    for(i=0; i<NUM_ROWS(cfg->gpio_default); i++)
    {
        cfg->gpio_default[i] = GP_UNINITIALIZED;
    } 
    
    // mqtt
    cfg->mqtt_user[0] = 0;
    cfg->mqtt_password[0] = 0;
    cfg->mqtt_broker_address[0] = 0;
}


// #define _GNU_SOURCE // Required for O_DIRECT on Linux
// #include <fcntl.h>
// #include <unistd.h>
// #include <stdlib.h>
// #include <stdio.h>

// int write_buffer_direct(const char* filename, size_t total_bytes) {
//     // 1. Allocate memory aligned to 4KB page boundaries
//     void* buffer = NULL;
//     if (posix_memalign(&buffer, 4096, total_bytes) != 0) {
//         perror("Failed to allocate aligned memory");
//         return -1;
//     }

//     // Fill your buffer with data here...

//     // 2. Open file with O_DIRECT to bypass OS page cache duplication
//     int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC /*| O_DIRECT*/, 0644);
//     if (fd < 0) {
//         perror("Failed to open file with O_DIRECT");
//         free(buffer);
//         return -1;
//     }

//     // 3. Write directly to disk (Zero-copy to page cache)
//     ssize_t bytes_written = write(fd, buffer, total_bytes);
//     if (bytes_written < 0) {
//         perror("Direct write failed");
//     }

//     close(fd);
//     free(buffer);
//     return (bytes_written == (ssize_t)total_bytes) ? 0 : -1;
// }


// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/mman.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <string.h>

// int mmap_test() 
// {
//     const char *filepath = "new_database.bin";
//     size_t FILE_SIZE = 4096; // 4 KB (typically matches 1 memory page)

//     // 1. Create and open the new file with Read/Write permissions
//     // O_CREAT: Create file if it doesn't exist.
//     // O_TRUNC: Truncate file to 0 bytes if it already exists.
//     int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0644);
//     if (fd == -1) {
//         perror("Error opening/creating file");
//         return EXIT_FAILURE;
//     }

//     // 2. STRETCH THE FILE: Set the storage space before calling mmap
//     // Memory mapping cannot dynamically increase the underlying file size.
//     if (ftruncate(fd, FILE_SIZE) == -1) {
//         perror("Error setting file size");
//         close(fd);
//         return EXIT_FAILURE;
//     }

//     // 3. Map the file into the process address space
//     // PROT_READ | PROT_WRITE: We want to read and write to this memory region.
//     // MAP_SHARED: Changes made to memory are automatically committed to the disk file.
//     char *map = picofs_mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
//     if (map == MAP_FAILED) {
//         perror("Error mapping the file");
//         close(fd);
//         return EXIT_FAILURE;
//     }

//     // The file descriptor can technically be closed right after mmap(), 
//     // but we will keep it standard and close it at the end.

//     // 4. Write data directly to the file using memory pointers
//     strcpy(map, "Hello, this is text written via mmap() memory manipulation!");
//     printf("Data written to memory map successfully.\n");

//     // 5. Optional: Synchronize memory changes back to disk immediately
//     // Without msync, the OS manages flushing, but msync forces durability.
//     // if (msync(map, FILE_SIZE, MS_SYNC) == -1) {
//     //     perror("Could not sync file to disk");
//     // }

//     // 6. Clean up: Unmap the memory and close the file descriptor
//     if (picofs_munmap(map, FILE_SIZE) == -1) {
//         perror("Error unmapping the memory");
//     }
    
//     close(fd);
//     return EXIT_SUCCESS;
// }
