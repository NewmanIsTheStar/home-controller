/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>


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

 
//int bytes_written = my_fs_write(&custom_fds[target_fd].my_fs_handle, ptr, len);

int picofs_close(int fd)
{
    int err = -1;
    int i;
    u8_t *erased_area;

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
        if (!picofs_find_contiguous_free_area(custom_fds[fd].file_header->file_size, &erased_area))
        {
            memcpy(erased_area, custom_fds[fd].cache, custom_fds[fd].file_header->file_size);
            err = 0;
        }
        else
        {
            printf("picoFS: out of space writing %s to flash\n", custom_fds[fd].file_header->name);
            err = -2;
        }

        vPortFree(custom_fds[fd].cache);
        custom_fds[fd].cache = NULL;
        custom_fds[fd].cache_len = 0;

        // clear out file descriptor
        custom_fds[fd].file = NULL;
        custom_fds[fd].file_len = 0;
        custom_fds[fd].file_header = NULL;
        custom_fds[fd].data = NULL;
        custom_fds[fd].data_len = 0;
        custom_fds[fd].data_offset = 0;
    }
    
    hex_dump((const char *)test_filesystem, sizeof(test_filesystem));
     
    return(err);
}

