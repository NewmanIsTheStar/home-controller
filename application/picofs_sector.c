/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "hardware/pio.h"
#include "hardware/clocks.h"
// #include "generated/ws2812.pio.h"

// Prune this list of includes
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>


#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

#include "stdarg.h"

// #include "weather.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"
// #include "led_strip.h"
#include "udp.h"
// #include "message.h"
// #include "message_defs.h"
// #include "powerwall.h"
#include "shelly.h"
#include "discovery_task.h"
#include "picofs.h"


//#define DEBUG_UDP_MESSAGES

//#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)





//prototypes
bool picofs_sector_in_use(u32_t sector);
bool picofs_sector_erased(u32_t sector);


// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif
extern FILE_STATUS_T picofs_files[FS_NUM_FID]; 
extern SemaphoreHandle_t picofs_mutex;

//static variables


/*!
 * \brief erase obsolete sectors
 * 
 *    
 * \return 0 on success
 */
int picofs_erase_obsolete_sectors(bool picofs_mutext_held)
{
    int err = -1;
    u32_t i;
    u32_t start_sector;
    u32_t end_sector;    


    if (picofs_mutext_held)
    {
        err = 0;
    }
    else if (xSemaphoreTake(picofs_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        err = 0;
    }

    if (!err)
    {
        picofs_refresh_files();

        for(start_sector=0; start_sector < FS_NUM_SECTORS; start_sector++)
        {
            if (!picofs_sector_in_use(start_sector) && !picofs_sector_erased(start_sector))
            {
                end_sector = start_sector;

                // try to expand range
                for (i = start_sector; i < FS_NUM_SECTORS; i++)
                {
                    if ((!picofs_sector_in_use(i)) && !picofs_sector_erased(i))
                    {
                        end_sector = i;
                    }
                    else
                    {
                        break;
                    }
                }

                if (start_sector == end_sector)
                {
                    //printf("--ERASING Sector %d\n", start_sector);
                    shell_printf("--ERASING Sector %d\n", start_sector);
                }
                else
                {
                    //printf("--ERASING Sectors %d to %d\n", start_sector, end_sector);
                    shell_printf("--ERASING Sectors %d to %d\n", start_sector, end_sector);
                }

                picofs_flash_erase_sector_range(start_sector, end_sector);
                start_sector = end_sector;
            }
        }

        if (!picofs_mutext_held)
        {
            xSemaphoreGive(picofs_mutex); 
        }
    }

    return(err);
}

/*!
 * \brief check if sector used by any file
 * 
 * \param[in]   sector     sector to check
 * 
 *     
 * \return 0 on success
 */
bool picofs_sector_in_use(u32_t sector)
{
    u32_t start_sector = 0;
    u32_t end_sector = 0;
    bool in_use = false;
    int i;
    
    // check flash
    for(i=0; i < FS_NUM_FID; i++)
    {
        if (picofs_files[i].valid && picofs_files[i].trailer)
        {
            start_sector = picofs_get_start_sector(picofs_files[i].trailer);

            end_sector = picofs_get_end_sector(picofs_files[i].trailer);

            if ((sector >= start_sector) && (sector <= end_sector))
            {
                // check if deleted file that is ready for erasure
                if (!(picofs_files[i].trailer->file_status && picofs_deleted_file_ready_for_erasure(picofs_files[i].trailer)))
                {
                    in_use= true;
                    break;
                }
            }
        }
    }

    // check open READ file descriptors (writes are in RAM cache and not affected by erasing flash sectors)
    for (int i = 0; i < FS_MAX_FILE_DESCRIPTORS; i++) 
    {
        if ((custom_fds[i].in_use) && (custom_fds[i].flags & FREAD) && (custom_fds[i].file_trailer))
        {
            start_sector = picofs_get_start_sector(custom_fds[i].file_trailer);

            end_sector = picofs_get_end_sector(custom_fds[i].file_trailer);

            if ((sector >= start_sector) && (sector <= end_sector))
            {
                in_use= true;
                break;
            }

            //TODO: atomically shift reader's FD to point to latest SEQ if the reader is using an obsolete copy due to consolidation
        }
    }

    return(in_use);
}

/*!
 * \brief check if sector is erased
 * 
 * \param[in]   sector     sector to check
 * 
 *     
 * \return 0 on success
 */
bool picofs_sector_erased(u32_t sector)
{
    u32_t start_sector = 0;
    u32_t end_sector = 0;
    bool erased = true;
    int i;
    char *cell;
    
    for(cell = (char *)(FLASH_SCAN_START + sector *FS_SECTOR_SIZE); cell < (char *)(FLASH_SCAN_START + (sector+1) *FS_SECTOR_SIZE); cell++)
    {        
        if (*cell != FS_ERASED_CELL_VALUE)
        {            
            erased = false;
            break;
        }
    }

    return(erased);
}

/*!
 * \brief check if sector is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  trailer      pointer to file trailer
 *  *     
 * \return 0 on success
 */
u32_t picofs_get_start_sector(FILE_TRAILER_T *trailer)
{
    u32_t start_block;
    
    start_block = ((((char *)trailer + sizeof(FILE_TRAILER_T)) - trailer->file_size) - FS_START)/FS_SECTOR_SIZE; 

    return(start_block);
}

/*!
 * \brief check if sector is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  trailer      pointer to file trailer
 *  *     
 * \return 0 on success
 */
u32_t picofs_get_end_sector(FILE_TRAILER_T *trailer)
{
    u32_t end_block;
    
    end_block = (((char *)trailer + sizeof(FILE_TRAILER_T) - 1) - FS_START)/FS_SECTOR_SIZE; 

    return(end_block);
}