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

//static variables



/*!
 * \brief resize file that is open for write
 *
 * \param fd     file descriptor
 * \return nothing
 */
int picofs_ftruncate(int fd, off_t length)
{
    int err = -1;
    size_t cache_size = 0;
    char *new_cache = NULL;

    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS) && (custom_fds[fd].flags & (O_WRONLY | O_RDWR)))
    {

        // sanity check
        if (length == custom_fds[fd].data_len)
        {
            return(0);
        }
        
        // allocate cache in multiples of sectors (4k)
        cache_size = ((length + (4*1024))/(4*1024))*(4*1024);        
        new_cache = pvPortMalloc(cache_size);

        if (new_cache && custom_fds[fd].cache)
        {
            if (length > custom_fds[fd].data_len)
            {
                // expand and pad with zeros
                memcpy(new_cache, custom_fds[fd].cache, custom_fds[fd].data_len);
                memset(new_cache+custom_fds[fd].data_len, 0, length-custom_fds[fd].data_len);
            }
            else
            {
                // truncate
                memcpy(new_cache, custom_fds[fd].cache, length);
            }

            // delete original cache
            vPortFree(custom_fds[fd].cache);
            //custom_fds[fd].cache = NULL;
        }
        else if (new_cache)
        {
            // no previous cache to copy from so zero the newly created cache
            memset(new_cache, 0, cache_size);
        }

        if (new_cache)
        {
            // point file descriptor to the new new cache
            custom_fds[fd].cache = new_cache;
            custom_fds[fd].cache_len = cache_size;
            custom_fds[fd].data = new_cache;
            custom_fds[fd].data_len = length;

            printf("truncate: new cache = %p data length = %d [cache size %d]\n", custom_fds[fd].cache, custom_fds[fd].data_len, custom_fds[fd].cache_len);
            err = 0;
        }
    }

    return(err);
}


