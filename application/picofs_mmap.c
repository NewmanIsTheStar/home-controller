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
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables


/*!
 * \brief crude mmap
 * 
 * \param addr     ignored -- set to NULL
 * \param len      number of bytes to map
 * \param prot     memory protection
 * \param flags    mapping attributes
 * \param fd       open file descriptor
 * \param offset   offset to start of mapping e.g. offset of zero maps from the start of file
 * \return pointer to mapping on success
 */
void *picofs_mmap(void *addr, size_t len, int prot, int flags, int fd, u32_t offset) 
{
    int target_fd = fd - 3;

    // handle RAM-only anonymous mappings
    if (flags & MAP_ANONYMOUS) 
    {
        if (!(prot & (PROT_READ | PROT_WRITE))) 
        {
            //errno = EINVAL;
            return(MAP_FAILED);
        }
        
        void *ptr = malloc(len);
        
        if (!ptr) 
        {
            //errno = ENOMEM;
            return(MAP_FAILED);
        }
        
        return ptr;
    }

    // handle read-only file mappings
    if (flags & MAP_SHARED)
    {
        if (prot & PROT_WRITE) 
        {
            //errno = EINVAL;
            return(MAP_FAILED);
        }

        if ((fd < 3) || (target_fd >= FS_MAX_FILE_DESCRIPTORS) || !custom_fds[target_fd].in_use)
        {
            //errno = EBADF;
            return(MAP_FAILED);
        }

        if (offset > custom_fds[target_fd].file_trailer->file_size - sizeof(FILE_TRAILER_T))
        {
            return(MAP_FAILED);
        }
        return((void *)(custom_fds[target_fd].file + offset));    // TODO add mmap status to fd so that we know file is in use after close
    }

    // handle read-only direct flash mappings
    if (prot & PROT_WRITE) 
    {
        // Microcontroller flash cannot be written directly via pointer like RAM
        //errno = EACCES; 
        return(MAP_FAILED);
    }

    // calculate direct pointer in XIP address space
    uintptr_t xip_address = XIP_BASE + offset;
    
    // Ensure bounds match standard flash boundaries (e.g., 2MB max for standard pico)
    if (offset + len > PICO_FLASH_SIZE_BYTES) 
    {
        //errno = ENXIO;
        return(MAP_FAILED);
    }

    return ((void *)xip_address);
}

/**
 * Unmaps the memory allocated or pointed to by pico_mmap
 */
int picofs_munmap(void *addr, size_t len) 
{
    uintptr_t address = (uintptr_t)addr;

    // if it points inside the Flash XIP window, nothing to free
    if (address >= XIP_BASE && address < (XIP_BASE + PICO_FLASH_SIZE_BYTES)) 
    {
        return(0); 
    }

    //TODO: release fd if it was held open by this mapping [flag in fd not implemented yet]

    // if it is in RAM, free the malloc'd buffer
    if (addr != NULL && addr != MAP_FAILED) 
    {
        free(addr);
        return(0);
    }

    //errno = EINVAL;
    return(-1);
}
