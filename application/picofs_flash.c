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

// TODO - prune this list of includes
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
typedef struct picofs_flash_range
{
    u32_t dst_offset;
    char *src;  
    size_t len;
} PICOFS_FLASH_RANGE_T;




//prototypes


// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern FILE_TEST_T test_filesystem[10];

//static variables



/*!
 * \brief Shim for programming flash pages with second core suspended and interrupts disabled
 * 
 * \param[in]   ptr flash offset, RAM source and data length 
 */
void __no_inline_not_in_flash_func(picofs_flash_program_shim)(void *ptr)
{
    PICOFS_FLASH_RANGE_T *parameters = ptr;

    if (ptr)
    {
        if (parameters->dst_offset%FS_PAGE_SIZE)
        {
            return;
        }

        if ((parameters->len%FS_PAGE_SIZE) || ((parameters->len + parameters->dst_offset) > FS_SIZE))
        {
            return;
        }

        // program the configuation in 256 byte pages (range is rounded up to the nearest multiple of 256 Bytes)
        #if FS_FAKE_FLASH
            memcpy((char *)(FS_FLASH_BASE+parameters->dst_offset), parameters->src, parameters->len);
        #else            
            flash_range_program(parameters->dst_offset, parameters->src, parameters->len);
        #endif
    }

    return;
}

/*!
 * \brief program flash pages (256 bytes)
 * 
 * \return 0 on success
 */
int picofs_flash_program(char *dst, char *src, size_t len)
{
    int err = 0;
    u32_t flash_offset = (u32_t)dst - (u32_t)FS_FLASH_BASE;
    PICOFS_FLASH_RANGE_T shim_parameters;

    if (flash_offset%FS_PAGE_SIZE)
    {
        printf("picofs: error: picofs_flash_program(): invalid page offset (%d)\n", flash_offset);
        return(-1);
    }

    if ((len%FS_PAGE_SIZE) || ((len+flash_offset) > FS_SIZE))
    {
        printf("picofs: error: picofs_flash_program(): invalid length (%d)\n", len);
        return(-2);
    }

    // prepare parameters for shim
    shim_parameters.dst_offset = flash_offset;
    shim_parameters.src = src;
    shim_parameters.len = len;

   
    err = flash_safe_execute(picofs_flash_program_shim, &shim_parameters, 5000);

    if (err)
    {
        printf("picofs: error programming flash: flash_safe_execute() returned error %d\n", err);

        #ifdef BREAKPOINT_FLASH_WRTIE_FAIL        
            // Hardcoded breakpoint instruction tells the SWD debugger to freeze right here
            __asm volatile("bkpt #0"); 
            while(1); // Infinite loop prevents the chip from executing further code/resetting
        #endif
    }
    else
    {
        // verify programming result
        err = memcmp(dst, src, len);

        if (err)
        {
            printf("picofs: error: picofs_flash_program(): verification failed\n");
        }
    }
    
    return(err);
}




/*!
 * \brief Shim for programming flash pages with second core suspended and interrupts disabled
 * 
 * \param[in]   ptr flash offset, RAM source and data length 
 */
void __no_inline_not_in_flash_func(picofs_flash_erase_shim)(void *ptr)
{
    PICOFS_FLASH_RANGE_T *parameters = ptr;

    if (ptr)
    {
        if (parameters->dst_offset%FS_PAGE_SIZE)
        {
            return;
        }

        if ((parameters->len%FS_PAGE_SIZE) || ((parameters->len + parameters->dst_offset) > FS_SIZE))
        {
            return;
        }

        // erase a flash block (4 KBytes)
        #if FS_FAKE_FLASH
            memset((char *)(FS_FLASH_BASE+parameters->dst_offset), FS_ERASED_CELL_VALUE, parameters->len);
        #else            
            flash_range_erase(parameters->dst_offset, parameters->src, parameters->len);
        #endif   

    }

    return;
}

/*!
 * \brief erase flash sectors (4096 bytes)
 * 
 * \return 0 on success
 */
int picofs_flash_erase(char *dst, size_t len)
{
    int err = 0;
    u32_t flash_offset = (u32_t)dst - (u32_t)FS_FLASH_BASE;
    PICOFS_FLASH_RANGE_T shim_parameters;    

    if (flash_offset%FS_SECTOR_SIZE)
    {
        printf("picofs: error: picofs_flash_erase(): invalid block offset\n");
        return(-1);
    }

    if ((len%FS_SECTOR_SIZE) || ((len+flash_offset) > FS_SIZE))
    {
        printf("picofs: error: picofs_flash_erase(): invalid length\n");
        return(-2);
    }
   
    // prepare parameters for shim
    shim_parameters.dst_offset = flash_offset;
    shim_parameters.src = NULL;
    shim_parameters.len = len;

   
    err = flash_safe_execute(picofs_flash_erase_shim, &shim_parameters, 5000);

    if (err)
    {
        printf("picofs: error erasing flash: flash_safe_execute() returned error %d\n", err); 
    }   

    return(err);
}

/*!
 * \brief erase a sequential set of sectors
 * 
 * \param[in]   start_sector     first sector to erase
 * 
 * \param[out]  end_sector       last sector to erase
 *  *     
 * \return 0 on success
 */
int picofs_flash_erase_sector_range(int start_sector, int end_sector)
{
    int err = 0;
    
    // #if FS_FAKE_FLASH
    //     memset(FS_START + start_sector*FS_SECTOR_SIZE, 255, (end_sector-start_sector+1)*FS_SECTOR_SIZE);
    // #else
    //     picofs_flash_erase(FS_START + start_sector*FS_SECTOR_SIZE, (end_sector-start_sector+1)*FS_SECTOR_SIZE);
    // #endif

    picofs_flash_erase(FS_START + start_sector*FS_SECTOR_SIZE, (end_sector-start_sector+1)*FS_SECTOR_SIZE);

    return(err);
}
