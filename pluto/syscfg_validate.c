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
#include "syscfg.h"
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

#include "syscfg.h"
#include "pluto.h"
#include "utility.h"

#include "flash.h"
#include "picofs.h"
#include "syscfg.h"

//#define DISABLE_SYSCFG_UPGRADE

// extern SYSTEM_CONVERSION_T syscfg_info[];
// extern int syscfg_info_rows;

/*!
 * \brief Check configuration is valid and upgrade if necessary 
 * 
 * \return 0 on success, -1 on error
 */
int syscfg_validate(char *filename, SYSTEM_CONVERSION_T conversion_table[], int conversion_table_rows, void **configuration_buffer)
{
    int err = 0;
    int i = 0;
    int version_from_flash = 0;
    uint16_t crc_from_flash = 0;
    uint16_t calculated_crc = 0;
    int latest_valid_syscfg_version = 0;
    void *previous_config = NULL;
    //CONFIG_TYPE_T syscfg_type;
    size_t biggest_config_size = 0;


    if (conversion_table_rows < 1)
    {
        printf("syscfg_validate: error no conversion table row found\n");
        return(-1);
    }

    // find the biggest version of the configuration
    for(i=0; i < conversion_table_rows; i++)
    {
        if (conversion_table[i].size > biggest_config_size)
        {
            biggest_config_size = conversion_table[i].size;
        }
    }

    // map configuration file into RAM and resize it to the biggest known configuration size 
    // this gives enough space for sequential conversion up to the latest configuration version
    err = syscfg_mmap(filename, configuration_buffer, biggest_config_size); 

    // find the configuration version that the file contains
    if (!err)
    {
        // check for valid configuration
        for(i=0; i < conversion_table_rows; i++)
        {
            version_from_flash = *((int *)((uint8_t *)*configuration_buffer + conversion_table[i].version_offset));
            crc_from_flash = *((uint16_t *)((uint8_t *)*configuration_buffer + conversion_table[i].crc_offset));
            calculated_crc = crc_buffer((uint8_t *)*configuration_buffer, conversion_table[i].crc_offset);        

            if ((version_from_flash == conversion_table[i].version) && (crc_from_flash == calculated_crc))
            {
                printf("Found valid configuration in %s with version %d\n", filename, version_from_flash);
                latest_valid_syscfg_version = version_from_flash;
                break;
            }
        }
    }
    

#ifndef DISABLE_SYSCFG_UPGRADE
    // obtain pointer to previous config if available (
    // this gives the conversion functions access to the original file in flash while modifying the cached version in RAM
    if (version_from_flash > 0)
    {
        previous_config = syscfg_get_flash_location(filename);
    }

    // upgrade configuration sequentially to latest version 
    for(i=0; i < conversion_table_rows; i++)
    {
        if (latest_valid_syscfg_version < conversion_table[i].version)
        {
            conversion_table[i].upgrade_function(previous_config);
        }
    }
#else
    if (latest_valid_syscfg_version < conversion_table[i].version)
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
 * \brief Check configuration is valid and upgrade if necessary 
 * 
 * \return +ve row number on success, -1 on error
 */
int syscfg_get_conversion_row(SYSTEM_CONVERSION_T conversion_table[], int conversion_table_rows, void *configuration_buffer)
{
    int row = -1;
    int i = 0;
    int version_from_flash = 0;

    // check for valid configuration
    for(i=0; i < conversion_table_rows; i++)
    {
        version_from_flash = *((int *)((uint8_t *)configuration_buffer + conversion_table[i].version_offset));      

        if ((version_from_flash == conversion_table[i].version))
        {
            row = i;
            break;
        }
    }

    return(row);
}
