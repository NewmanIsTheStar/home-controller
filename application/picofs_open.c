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
// FILE_TEST_T test_filesystem[10];
 
// #define FS_FLASH_START ((char *)(&test_filesystem))
// #define FS_FLASH_END ((char *)(&test_filesystem) + sizeof(test_filesystem))
// #define FS_VERION (0)


/*
name: A pointer to a null-terminated string specifying the path to the file you want to open.
flags: A bitwise OR mask (|) determining the file access mode and operational behaviors.
mode: An optional argument (typically an octal number or permission macros) used only when a new file is being created (via O_CREAT or O_TMPFILE). 
If neither flag is provided, this parameter is ignored.

Common Flags (flags)The access mode must include exactly one of the following core options:
O_RDONLY: Open for reading only.
O_WRONLY: Open for writing only.
O_RDWR: Open for both reading and writing.

You can bitwise OR (|) these with additional file creation or status flags:
O_CREAT: Create the file if it does not exist.
O_TRUNC: Truncate the file length to 0 if it already exists and is opened for writing.
O_APPEND: Move the file offset pointer to the end of the file before every write.
O_EXCL: Ensure that this call creates the file; if the file already exists, the call fails (used alongside O_CREAT).
*/

/*!
 * \brief open a file
 *
 * \param fd     file descriptor
 * \param name   file name string
 * \param flags  bitwise flags O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND, O_EXCL 
 * \return nothing
 */
int picofs_open(int fd, char *name, int flags)
{
    int err = -1;
    char *file_header = NULL;
    char *file_data = NULL;

    if (picofs_find_by_name(name, &file_header))
    {
        // file exists
        if (flags & O_RDONLY)
        {
            err = 0;

            picofs_fd_initialize(fd, (FILE_HEADER_T *)file_header);
        } 
        else if ((flags & O_WRONLY) || (flags & O_RDWR))
        {
            // need exclusive access for write
            if (!picofs_file_in_use(file_header))
            {
                picofs_fd_initialize(fd, (FILE_HEADER_T *)file_header);
                err = picofs_allocate_cache(fd);
                
                if (!err)
                {
                    if (custom_fds[fd].cache_len >= custom_fds[fd].file_len)
                    {
                        // populate cache
                        memcpy(custom_fds[fd].cache, custom_fds[fd].file, custom_fds[fd].file_len);
                    }
                    else
                    {
                        err = -3;
                    }
                }
            }

            if (!err && (flags & O_APPEND))
            {
                custom_fds[fd].file_offset = custom_fds[fd].file_len;
            }

            if (!err && (flags & O_TRUNC))
            {
                custom_fds[fd].file_offset = 0;
                custom_fds[fd].file_len = 0;

                // need to update file header in RAM
            }            
        }        
    }
    else
    {
        // file does not exit
        if (((flags & O_WRONLY) || (flags & O_RDWR)) && (flags & O_CREAT))
        {
            picofs_fd_initialize(fd, NULL);
            err = picofs_allocate_cache(fd);

            //need to generate file header in RAM
        }
    }

    return(err);
}


/*!
 * \brief allocate RAM cache for file writes
 *
 * \param fd     file descriptor
 * \param header file header
 * \return nothing
 */
int picofs_fd_initialize(int fd, FILE_HEADER_T *header)
{
    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {
        if (header)
        {
            custom_fds[fd].file = (char *)header;
            custom_fds[fd].file_len = header->file_size;
            custom_fds[fd].file_offset = 0;
        }
        else
        {
            custom_fds[fd].file = NULL;
            custom_fds[fd].file_len = 0;
            custom_fds[fd].file_offset = 0;
        }
    }

    return(0);
}

/*!
 * \brief allocate RAM cache for file writes
 *
 * \param fd     file descriptor
 * \return nothing
 */
int picofs_allocate_cache(int fd)
{
    int err = -1;
    size_t cache_size = 0;

    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {
        if (custom_fds[fd].file)
        {
            custom_fds[fd].file_len = ((FILE_HEADER_T *)custom_fds[fd].file)->file_size;
        }
        else
        {
            custom_fds[fd].file_len = 0;
        }

        // allocate one 4k block greater than currently used
        cache_size = ((custom_fds[fd].file_len + (4*1024))/(4*1024))*(4*1024);

        custom_fds[fd].cache = pvPortMalloc(cache_size);

        if (custom_fds[fd].cache != NULL)
        {
            custom_fds[fd].cache_len = cache_size;

            err = 0;            
        }
    }

    return(err);
}


/*!
 * \brief check if a file is already open
 *
 * \param file_header pointer to file header
 * \return nothing
 */
bool picofs_file_in_use(char *file_header)
{
    int i;
    bool in_use = false;

    for(i=0; i < FS_MAX_FILE_DESCRIPTORS; i++)
    {
        if (file_header && (file_header == custom_fds[i].file) && custom_fds[i].in_use)
        {
            in_use= true;
            break;
        }
    }

    return(in_use);
}

/*!
 * \brief Get pointer to file header matching passed filename
 * 
 * \param[in]   filename     name to find
 * \param[out]  header       pointer to file header
 * \return 0 on success
 */
int picofs_find_by_name(char *filename, char **header)
{
    int err = -1;
    u8_t *p = FS_FLASH_START;
    //  u8_t *next = NULL; 
    FILE_HEADER_T *h = NULL;
    u8_t best_sequence = 0;
    bool first_sequnce = false;

    // TEST TEST TEST
    picofs_load_test_data();

    while (((char *)p) < FS_FLASH_END)
    {
        h = (FILE_HEADER_T *)p;

        if ((strncmp(h->magic_number, "pfs", 4) == 0) &&
            (h->picofs_version == FS_VERION) &&
            (strcmp(h->name, filename) == 0))
        {
            // match
            if (!first_sequnce)
            {
                best_sequence = h->file_sequence;
                *header = (char *)h;
                err = 0;
                p = p + sizeof(FILE_HEADER_T) + h->file_size + h->file_padding + sizeof(FILE_TRAILER_T);
            }
            else
            {
                if ((h->file_sequence - best_sequence) < 128)
                {
                    best_sequence = h->file_sequence;
                    *header = (char *)h;
                    err = 0;
                    p = p + sizeof(FILE_HEADER_T) + h->file_size + h->file_padding + sizeof(FILE_TRAILER_T);                  
                }
            }
        }

        if (p == ((u8_t *)h))
        {
            p++;
        }
    }


    return(err);
}