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


//#define DISABLE_SYSCFG_WRITE [1]

extern SYSTEM_CONVERSION_T syscfg_info[];
extern int syscfg_info_rows;

// prototypes
int syscfg_sync_changes(char *filename, void *configuration_buffer, int configuration_len, SYSTEM_CONVERSION_T conversion_table[], int conversion_table_rows);

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

        syscfg_sync_changes("system.cfg", sys, sizeof(SYSTEM_CONFIG_T), syscfg_info, syscfg_info_rows);
    }  
    #endif

    return(err);
}


/*!
 * \brief Copy the configuration from RAM into flash if they differ.
 * 
 * \return 0 on success, -1 on error
 */
int syscfg_sync_changes(char *filename, void *configuration_buffer, int configuration_len, SYSTEM_CONVERSION_T conversion_table[], int conversion_table_rows)
{
    int err = 0;
    int row = -1;


    row = syscfg_get_conversion_row(conversion_table, conversion_table_rows, configuration_buffer);

    if (row < 0)
    {
        printf("syscfg_sync_changes: error no conversion table row found\n");
        return(-1);
    }

    // update crc        
    *((uint16_t *)((uint8_t *)configuration_buffer + conversion_table[row].crc_offset)) = crc_buffer((uint8_t *)configuration_buffer, conversion_table[row].crc_offset); 
        
    // compare ram and flash copies
    if (syscfg_compare_flash_ram(filename, configuration_buffer, false, false))
    {
        printf("Writing configuration to flash\n");

        if (err = syscfg_sync_file(configuration_buffer, configuration_len))
        {
            printf("Failed to write system configuration to flash (%d)\n", err);                
        } 
        else if (syscfg_compare_flash_ram(filename, configuration_buffer, false, true))  // we just wrote the config so there shoud now be no differences
        {
            printf("syscfg_write: DUMPING SYSTEM CONFIG because difference found after writing to flash!\n");
            hex_dump((const uint8_t *)sys, sizeof(SYSTEM_CONFIG_T));
        }          
    }           
    else
    {
        printf("Refusing to write system configuration to flash as RAM and flash copies are identical\n");
    }

    // check for collision
    if (*((uint16_t *)((uint8_t *)configuration_buffer + conversion_table[row].crc_offset)) != crc_buffer((uint8_t *)configuration_buffer, conversion_table[row].crc_offset))
    {
        // config was updated by another task after we computed the crc and possibly before we wrote to flash
        printf("System configuration update occured while writing to flash, will retry\n");
        
        syscfg_changed();

        err = -1;
    }          
     


    return(err);
}

/*!
 * \brief Compare flash and RAM copies of configuration
 * 
 * \return 0 = no difference, 1 = difference
 */
bool syscfg_compare_flash_ram(char *filename, void *configuration_buffer, bool stop_at_first_difference, bool print_differences)
{
    int i;
    bool difference_found = false;
    char *syscfg_location_in_flash = NULL;

    syscfg_location_in_flash = syscfg_get_flash_location(filename);

    if (syscfg_location_in_flash)
    {
        if (print_differences)
        {
            for (i=0; i<sizeof(SYSTEM_CONFIG_T); i++)
            {
                if (syscfg_location_in_flash[i] != ((char *)configuration_buffer)[i])
                {
                    if (!difference_found)
                    {
                        // printf headings
                        printf("     offset\tflash\tram\n");
                    }

                    // print difference
                    printf("%08x:\t%02x \t%02x\n", i, syscfg_location_in_flash[i], ((char *)configuration_buffer)[i]);
                    
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
            if (memcmp(syscfg_location_in_flash, ((char *)configuration_buffer), sizeof(SYSTEM_CONFIG_T)))
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



