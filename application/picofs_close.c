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





//prototypes


// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern FILE_TEST_T test_filesystem[10];
//static variables

 
/*!
 * \brief close file 
 *
 * \param fd     file descriptor
 * \return 0 on success
 */
int picofs_close(int fd)
{
    int err = -1;
    int i;
    u8_t *erased_area;
    int padding_len = 0;

    if (!((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS)))
    {
        return(err);
    }

    if (!custom_fds[fd].in_use)
    {
        return(err);
    }

    if (custom_fds[fd].cache)
    {
        // update trailer

        // set size and status
        custom_fds[fd].cache_trailer.file_size = custom_fds[fd].data_len + sizeof(FILE_TRAILER_T);
        custom_fds[fd].cache_trailer.file_status = custom_fds[fd].file_status;

        // append trailer to end of cached file 
        if ((custom_fds[fd].data_len + sizeof(FILE_TRAILER_T)) < custom_fds[fd].cache_len)
        {
            memcpy(custom_fds[fd].cache + custom_fds[fd].data_len, &(custom_fds[fd].cache_trailer), sizeof(FILE_TRAILER_T));
            err = 0;
        }
        else
        {
            shell_printf("picoFS: out of cache appending trailer to %s for write to flash\n",custom_fds[fd].cache_trailer.name);
            err = -2;            
        }

        // pad cache with consolidated files
        if (!err)
        {
            padding_len = custom_fds[fd].cache_trailer.file_size%256?(256 - custom_fds[fd].cache_trailer.file_size%256):0;

            if (padding_len)
            {
                picofs_consolidate_files_to_buffer(custom_fds[fd].cache + custom_fds[fd].cache_trailer.file_size, padding_len, custom_fds[fd].cache_trailer.file_id);
            }
        }

        if (!picofs_find_contiguous_free_area(custom_fds[fd].cache_trailer.file_size, &erased_area) && (err == 0))
        {
            // printf("about to copy out of cache into flash\n");
            // hex_dump(custom_fds[fd].cache, h->file_size);
            memcpy(erased_area, custom_fds[fd].cache, custom_fds[fd].cache_trailer.file_size + padding_len);
            err = 0;
        }
        else
        {
            shell_printf("picoFS: out of space writing %s to flash\n",custom_fds[fd].cache_trailer.name);
            err = -2;
        }

        // clear the cache
        picofs_deallocate_cache(fd);

        // clear out the remainder of the file descriptor
        custom_fds[fd].file = NULL;
        custom_fds[fd].file_len = 0;
        custom_fds[fd].file_trailer = NULL;
        custom_fds[fd].data = NULL;
        custom_fds[fd].data_len = 0;
        custom_fds[fd].data_offset = 0;
    }
    
    //hex_dump((const char *)test_filesystem, sizeof(test_filesystem));
     
    return(err);
}

/*!
 * \brief deallocate write cache 
 *
 * \param fd     file descriptor
 * \return nothing
 */
int picofs_deallocate_cache(int fd)
{
    int err = 0;

    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {
        // free cache memory
        vPortFree(custom_fds[fd].cache);
        custom_fds[fd].cache = NULL;

        // clear the cache        
        custom_fds[fd].cache_len = 0;
        memset(&custom_fds[fd].cache_trailer, 0, sizeof(FILE_TRAILER_T));
    }

    return(err);
}