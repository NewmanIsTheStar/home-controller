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
int picofs_find_available_fd(void);
int picofs_release_fd(int fd);

// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern FILE_TEST_T test_filesystem[10];
//static variables

 
/*!
 * \brief copy file 
 *
 * \param fd     file descriptor 
 * \return 0 on success
 */
int picofs_copy(char *src, char *dst)
{
    int err = -1;
    int i;
    int fd;
    u8_t file_id = 255;
    u8_t file_sequence = 0;
    FILE_TRAILER_T *existing_dst = NULL;


    fd = picofs_find_available_fd();   // NB we are bypassing the wrappers so have to manage file descriptors directly in this function

    if (!((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS)))
    {
        return(err);
    }

    if (picofs_open(fd, src, 2))
    {
        errno = ENOENT; // File not found
        return -1;
    }    
    
    custom_fds[fd].in_use = true;  // this cannot occur before open because open uses this flag to determine if it has exlusive access to the file for write

    // check if destination filename already exists
    if (picofs_find_by_name(dst, &existing_dst))
    {
        // create new file
        file_id = picofs_get_new_file_id();
        file_sequence = 0;

        if (file_id == 255)
        {
            picofs_release_fd(fd);
            return -2;        
        }
    }
    else
    {
        // reuse existing destination file
        if (!existing_dst)
        {
            picofs_release_fd(fd);
            return -3;               
        }

        file_id = existing_dst->file_id;
        file_sequence = existing_dst->file_sequence + 1;
    }

    // assign new file id and name
    custom_fds[fd].cache_trailer.file_id = file_id;
    custom_fds[fd].cache_trailer.file_sequence = file_sequence;
    STRNCPY(custom_fds[fd].cache_trailer.name, dst, sizeof(custom_fds[fd].cache_trailer.name));

    picofs_close(fd);
    picofs_release_fd(fd);

    return(err);
}

int picofs_find_available_fd(void) 
{
    // find a free slot in custom_fds
    int fd = -1;
    for (int i = 0; i < FS_MAX_FILE_DESCRIPTORS; i++) 
    {
        if (!custom_fds[i].in_use) 
        {
            fd = i;
            //custom_fds[fd].in_use = true;  DON'T DO IT! caller must set this at the approriate time to avoid being blocked by sanity checks
            break;
        }
    }
    
    if (fd == -1) 
    {
        errno = ENFILE; // Too many open files
        return -1;
    }

    return(fd);
}

int picofs_release_fd(int fd) 
{
    if (fd >= FS_MAX_FILE_DESCRIPTORS || !custom_fds[fd].in_use)
    {
        return -1;
    }

    custom_fds[fd].in_use = false;

    return(0);
}