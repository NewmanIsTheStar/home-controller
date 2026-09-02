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
int picofs_get_fd_from_mmap_address(void *addr) ;

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
            errno = EINVAL;
            return(MAP_FAILED);
        }
        
        void *ptr = malloc(len);
        
        if (!ptr) 
        {
            errno = ENOMEM;
            return(MAP_FAILED);
        }
        
        return ptr;
    }

    // handle file mappings
    if (flags & MAP_SHARED)
    {
        if ((fd < 3) || (target_fd >= FS_MAX_FILE_DESCRIPTORS) || !custom_fds[target_fd].in_use)
        {
            errno = EBADF;
            return(MAP_FAILED);
        }

        if (prot & PROT_WRITE) 
        {
            if (offset > (custom_fds[target_fd].data_len))
            {
                return(MAP_FAILED);
            }

            custom_fds[fd].mmap_ref_count++;

            return((void *)(custom_fds[target_fd].cache + offset));
        }
        else if (prot & PROT_READ)
        {
            if (offset > (custom_fds[target_fd].data_len))
            {
                return(MAP_FAILED);
            }

            custom_fds[fd].mmap_ref_count++;

            return((void *)(custom_fds[target_fd].file + offset));
        }        
    }

    // handle read-only direct flash mappings
    if (prot & PROT_WRITE) 
    {
        // microcontroller flash cannot be written directly via pointer like RAM
        errno = EACCES; 
        return(MAP_FAILED);
    }

    // calculate direct pointer in XIP address space
    uintptr_t xip_address = XIP_BASE + offset;
    
    // ensure bounds match standard flash boundaries
    if (offset + len > PICO_FLASH_SIZE_BYTES) 
    {
        errno = ENXIO;
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
    int fd = -1;

    // if it points inside the Flash XIP window, nothing to free
    if (address >= XIP_BASE && address < (XIP_BASE + PICO_FLASH_SIZE_BYTES)) 
    {
        return(0); 
    }
     
    fd = picofs_get_fd_from_mmap_address(addr);

    if ((fd >= 0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {
        // RAM associated with a file descriptor (i.e. points within the cache)
        if (custom_fds[fd].mmap_ref_count > 0)
        {
            custom_fds[fd].mmap_ref_count--;
        }

        //TODO: release fd if it file was closed but fd was held open by this mapping 
        
        return(0);
    }
    else
    {
        // RAM not associated with a file descriptor so assume anonymouse mapping and free the malloc'd buffer
        if (addr != NULL && addr != MAP_FAILED) 
        {
            free(addr);
            return(0);
        }
    }

    //errno = EINVAL;
    return(-1);
}


int picofs_get_fd_from_mmap_address(void *addr) 
{
    int i;
    int fd = -1;

    for(i=0; i < FS_MAX_FILE_DESCRIPTORS; i++)
    {
        if ((addr >= (void *)custom_fds[i].cache) && (addr < (void *)(custom_fds[i].cache + custom_fds[i].cache_len)))
        {
            fd = i;
            break;
        }
    }
    
    return(fd);
}

int picofs_msync(void *addr, size_t length, int flags)
{
    int fd = -1;
    int fid = FS_INVALID_FID;
    
    fd = picofs_get_fd_from_mmap_address(addr);

    if ((fd >= 0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {
        if (!picofs_sync_file(fd, false))
        {
            // remember fid in case of rollover
            fid = custom_fds[fd].cache_trailer.file_id;

            // increment sequence number 
            picofs_increment_sequence(&(custom_fds[fd].cache_trailer));

            if (custom_fds[fd].cache_trailer.file_id != fid)
            {
                // rollover occured to a new fid so delete file with old fid
                //picofs_unlink_by_fid(fid);
                custom_fds[fd].rollover_fid = fid;
            }
        }
    }
}