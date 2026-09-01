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



// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables



/*!
 * \brief Identify the status of each flash page
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_find_page_status(PFS_DISPLAY_TYPE_T display)
{
    char *cell = NULL;
    u32_t erase_sector_absolute = 0;
    u32_t erase_sector_relative = 0;    
    u32_t page_relative = 0;
    u32_t free_pages = 0;
    u32_t total_pages = 0;
    TickType_t start_tick;
    TickType_t elapsed_ticks = 0;
    FILE_TRAILER_T *trailer = NULL;
    u8_t file_id = FS_INVALID_FID;
    u8_t file_sequence = 0;

    start_tick = xTaskGetTickCount();

    for(cell = FLASH_SCAN_START; cell < FLASH_SCAN_END;)
    {

        erase_sector_absolute = ((u32_t)cell)/FS_SECTOR_SIZE;
        erase_sector_relative = (u32_t)(cell - FLASH_SCAN_START)/FS_SECTOR_SIZE;            
        page_relative = ((u32_t)(cell - FLASH_SCAN_START)%FS_SECTOR_SIZE)/FS_PAGE_SIZE;  
        

        switch(display)
        {
        case PFS_DISPLAY_PAGE_MAP:
            if (((((u32_t)(cell - FLASH_SCAN_START))%FS_SECTOR_SIZE) == 0))
            {
                printf("\n[Sector%04d]", erase_sector_relative);
            }        
            break;
        case PFS_DISPLAY_SHELL_PAGE_MAP:
            if (((((u32_t)(cell - FLASH_SCAN_START))%FS_SECTOR_SIZE) == 0))
            {
                shell_printf("\n[Sector%04d]", erase_sector_relative);
            }  
            break;
        default:
            break;
        }

        if (*cell != FS_ERASED_CELL_VALUE)
        {
            switch(display)
            {
            default:
            case PFS_DISPLAY_QUIET:
                break;
            case PFS_DISPLAY_PAGE_NUMBERS:
                // file_id = picofs_find_file_at_location(cell, &trailer);
                // if (trailer)
                // {
                //     file_sequence = trailer->file_sequence;
                // }
                // else
                // {
                //     file_sequence = 0;
                // }
                printf("[Sector%04d, Page%02d]\tused\n", erase_sector_relative, page_relative);
                break;
            case PFS_DISPLAY_PAGE_MAP:
                printf("\u25A0");
                break;
            case PFS_DISPLAY_SHELL_PAGE_NUMBERS:
                // file_id = picofs_find_file_at_location(cell, &trailer);
                // if (trailer)
                // {
                //     file_sequence = trailer->file_sequence;
                // }
                // else
                // {
                //     file_sequence = 0;
                // }
                shell_printf("[Sector%04d, Page%02d]\tused\n", erase_sector_relative, page_relative);
                //hex_dump((char *)test_filesystem, 512);
                break;            
            case PFS_DISPLAY_SHELL_PAGE_MAP:
                shell_printf("\u25A0");
                break;
            }
            
            // skip to next page
            cell = FLASH_SCAN_START + erase_sector_relative*FS_SECTOR_SIZE+((page_relative+1)*FS_PAGE_SIZE);
        }
        else if (((cell - FLASH_SCAN_START)%FS_PAGE_SIZE) == (FS_PAGE_SIZE-1))  // reached last cell of page
        {
            switch(display)
            {
            default:
            case PFS_DISPLAY_QUIET:
                break;
            case PFS_DISPLAY_PAGE_NUMBERS:
                printf("[Sector%04d, Page%02d]\tfree\n", erase_sector_relative, page_relative);
                break;
            case PFS_DISPLAY_PAGE_MAP:
                printf("\u25A1");
                break;
            case PFS_DISPLAY_SHELL_PAGE_NUMBERS:
                shell_printf("[Sector%04d, Page%02d]\tfree\n", erase_sector_relative, page_relative);
                break;            
            case PFS_DISPLAY_SHELL_PAGE_MAP:
                shell_printf("\u25A1");
                break;                            
            }            
                        
            free_pages++;
            cell++;            
        }
        else
        {
            cell++;
        }
    }

    switch(display)
    {
    case PFS_DISPLAY_PAGE_MAP:
        printf("\n");      
        break;
    case PFS_DISPLAY_SHELL_PAGE_MAP:
        shell_printf("\n");
        break;
    default:
        break;
    }    

    elapsed_ticks = xTaskGetTickCount() - start_tick;
    total_pages = (FLASH_SCAN_END - FLASH_SCAN_START)/FS_PAGE_SIZE;

    switch(display)
    {
    default:
    case PFS_DISPLAY_QUIET:
        break;
    case PFS_DISPLAY_PAGE_NUMBERS:
    case PFS_DISPLAY_PAGE_MAP:
        printf("Free Pages = %d out of %d total pages (%d%%)\n", free_pages, total_pages, (free_pages*100)/total_pages);
        printf("flash scan completed in %d ms\n", elapsed_ticks);
        break;
    case PFS_DISPLAY_SHELL_PAGE_NUMBERS:          
    case PFS_DISPLAY_SHELL_PAGE_MAP:
        shell_printf("Free Pages = %d out of %d total pages (%d%%)\n", free_pages, total_pages, (free_pages*100)/total_pages);
        shell_printf("flash scan completed in %d ms\n", elapsed_ticks);
        break;                          
    }   

    return(0);
}

/*!
 * \brief Identify contiguous erased area large enough to hold size bytes
 * 
 * \param[in]   size             number of bytes
 * 
 * \param[out]  start_of_area    pointer to found area
 *  *     
 * \return 0 on success
 */
int picofs_find_contiguous_free_area(size_t requested_size, u8_t **start_of_area, size_t *actual_size)
{
    int err = 1;
    char *cell = NULL;
    u32_t erase_block_absolute = 0;
    u32_t erase_block_relative = 0;    
    u32_t page_relative = 0;
    u32_t free_pages = 0;
    u32_t total_pages = 0;
    TickType_t start_tick;
    TickType_t elapsed_ticks = 0;
    u32_t contiguous_pages_required = 0;
    u32_t contiguous_pages_found = 0;    

    start_tick = xTaskGetTickCount();

    contiguous_pages_required = requested_size/FS_PAGE_SIZE + (requested_size%FS_PAGE_SIZE?1:0);
    //printf("seeking %d contiguous pages\n", contiguous_pages_required);

    for(cell = *start_of_area = FLASH_SCAN_START; cell < FLASH_SCAN_END;)
    {
        erase_block_absolute = ((u32_t)cell)/FS_SECTOR_SIZE;
        erase_block_relative = (u32_t)(cell - FLASH_SCAN_START)/FS_SECTOR_SIZE;            
        page_relative = ((u32_t)(cell - FLASH_SCAN_START)%FS_SECTOR_SIZE)/FS_PAGE_SIZE;          
        
        if (*cell != FS_ERASED_CELL_VALUE)
        {            
            // skip to next page
            cell = FLASH_SCAN_START + erase_block_relative*FS_SECTOR_SIZE+((page_relative+1)*FS_PAGE_SIZE);

            // reset counter
            contiguous_pages_found = 0;

            // remember start address
            *start_of_area = cell;
        }
        else if (((cell - FLASH_SCAN_START)%FS_PAGE_SIZE) == (FS_PAGE_SIZE-1))
        {                                  
            contiguous_pages_found++;
            free_pages++;
            cell++;            

            if (contiguous_pages_found == contiguous_pages_required)
            {
                // check for additional erased space beyond that requested 
                for( ; (cell < FLASH_SCAN_END) && (*cell == FS_ERASED_CELL_VALUE); cell++);

                *actual_size = (size_t)(cell - (char *)*start_of_area);

                //printf("Contiguous area: request %d actual %d\n", requested_size, *actual_size);
                err = 0;
                break;
            }
        }
        else
        {
            cell++;
        }
    }

    elapsed_ticks = xTaskGetTickCount() - start_tick;
    
    if (!err)
    {
        // printf("Found erased region starting @%p with size %d in %d ms\n", *start_of_area, size, elapsed_ticks);
        // hex_dump(*start_of_area, 16);
    }
    else
    {
        // printf("failed to find erased pages\n");
        *start_of_area = NULL;
        *actual_size = 0;
    }
    
    return(err);
}


