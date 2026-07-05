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

//static variables


int picofs_write(int fd, char *ptr, int len)
{
    int err = 0;
    int i;

    if (custom_fds[fd].flags & O_APPEND)
    {
        // move data_offset to end of file before each write
        custom_fds[fd].data_offset = custom_fds[fd].data_len;
    }

    for(i=0; i<len; i++)
    {       
        if ((custom_fds[fd].data_offset + i) < (custom_fds[fd].cache_len - sizeof(FILE_TRAILER_T)))
        {
            custom_fds[fd].data[custom_fds[fd].data_offset + i] = ptr[i];
            
        }
        else
        {
            printf("picoFS: write truncated, out of cache\n");
            err = -1;
            break;
        }
    }

    custom_fds[fd].data_offset += i; 

    // check if write increase data length
    if (custom_fds[fd].data_offset > custom_fds[fd].data_len)
    {
        // increase data length to match offset 
        custom_fds[fd].data_len = custom_fds[fd].data_offset;
    }

    if (err)
    {
        // return the error code rather than bytes written
        i = err;
    }

    return(i);
}

