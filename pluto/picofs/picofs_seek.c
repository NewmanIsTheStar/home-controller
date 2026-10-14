/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

//#include <errno.h>
#include <fcntl.h>
#include "pico/cyw43_arch.h"

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

#include "FreeRTOS.h"
#include "semphr.h"
// #include "shell.h"
#include "picofs.h"
// #include "pluto.h"


//prototypes

// external variables
// extern uint32_t unix_time;
// extern APP_CONFIG_T config;
// extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];

//static variables
// AD BD Oct 16
/*
• EBADF: The file descriptor (fd) is invalid or not open.
• EINVAL: The whence (or dir) argument is invalid, or the resulting file offset would be negative (e.g., trying to seek backward past the start of the file).
• ESPIPE: The file descriptor is associated with a pipe, socket, or FIFO, which do not support seeking.
*/
int picofs_seek(int fd, int ptr, int dir)
{
    int err = 0;
    int i;
    int cache_index = 0;
    size_t requested_offset = 0;

    // TODO: a *lot* of bounds checking and memory allocation! (i.e. seeking outside range of existing file)
    switch(dir)
    {
    default:
    case SEEK_SET:
        requested_offset = ptr;
        break;
    case SEEK_CUR:
        if (ptr >= 0)
        {
            requested_offset = custom_fds[fd].data_offset + ptr;
        }
        else
        {
            if (abs(ptr) <= custom_fds[fd].data_offset)
            {
                requested_offset = custom_fds[fd].data_offset + ptr;
            }
            else
            {
                // clip at zero -- expanding backwards beyond start of file is not supported
                requested_offset = 0;
                err = -1; 
                errno = EINVAL;
            }
        }
        break;
    case SEEK_END: 
        if (ptr >= 0)
        {
            requested_offset = custom_fds[fd].data_len + ptr;
        }
        else
        {
            if (abs(ptr) <= custom_fds[fd].data_len)
            {
                requested_offset = custom_fds[fd].data_len + ptr;
            }
            else
            {
                // clip at zero -- expanding backwards beyond start of file is not supported
                requested_offset = 0;
                err = -2;
                errno = EINVAL; 
            }
        }
        break;
    }

    if (!picofs_cache_contains_entire_file(fd))
    {
        // check if new position is within the existing cache
        if ((requested_offset >= custom_fds[fd].cache_offset) && (requested_offset < custom_fds[fd].cache_offset + custom_fds[fd].cache_len))
        {
            custom_fds[fd].cache_offset = requested_offset;
        }
        else // need to move cache window
        {
            cache_index = custom_fds[fd].data_offset - custom_fds[fd].cache_offset;

            if (cache_index%FS_PAGE_SIZE)
            {
                picofs_printf("picofs: seek error : attempt to move data offset while cache contains an incomplete flash page\n");
                err = -3;
                errno = EINVAL;
            }
            else
            {
                if ((custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset + cache_index) <= custom_fds[fd].reserved_flash_end)
                {
                    //picofs_printf("writing to flash @ %0x do+i = %0x len = %0x\n", custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset, custom_fds[fd].data_offset+i, custom_fds[fd].cache_len);
                    // write the cache to flash
                    picofs_flash_program(custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset, custom_fds[fd].cache, cache_index);

                    // clear the cache 
                    memset(custom_fds[fd].cache, FS_ERASED_CELL_VALUE, custom_fds[fd].cache_len);

                    // shift the cache window 
                    custom_fds[fd].cache_offset = requested_offset;  
                    custom_fds[fd].data_offset = requested_offset;            
                }
            }
        }

        // check if seek exceeded flash reservation
        if (requested_offset >= (size_t)(custom_fds[fd].reserved_flash_end - custom_fds[fd].reserved_flash_start))
        {
            // we do not allow the size of large files to be altered via seek
            // large files must be truncated and overwritten entirely            
            picofs_printf("picofs: seek error : attempt to use seek to expand the size of a large file (not supported)\n");
            custom_fds[fd].data_offset = (size_t)(custom_fds[fd].reserved_flash_end - custom_fds[fd].reserved_flash_start);

            err = -4;
            errno = EINVAL;
        }
    }

    if (!err)
    {
        custom_fds[fd].data_offset = requested_offset;
        return custom_fds[fd].data_offset;
    }
    else
    {
        return(-1);
    }
}


