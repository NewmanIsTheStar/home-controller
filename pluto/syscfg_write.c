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
        sys->crc = crc_buffer((uint8_t *)sys, offsetof(SYSTEM_CONFIG_T, crc)); 
         
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
                hex_dump((const uint8_t *)sys, sizeof(SYSTEM_CONFIG_T));
            }          
        }           
        else
        {
            printf("Refusing to write system configuration to flash as RAM and flash copies are identical\n");
        }

        // check for collision
        if (sys->crc != crc_buffer((uint8_t *)sys, offsetof(SYSTEM_CONFIG_T, crc)))
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
            for (i=0; i<sizeof(SYSTEM_CONFIG_T); i++)
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
            if (memcmp(syscfg_location_in_flash, ((char *)sys), sizeof(SYSTEM_CONFIG_T)))
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



