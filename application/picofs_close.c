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
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];

//static variables
FILE_METRICS_T purge_list[FS_NUM_FID]; 
 
/*!
 * \brief close file 
 *
 * \param fd              file descriptor
 * \param disable_purge   do not purge duplicate filenames (only used when already executing a purge)
 * \return 0 on success
 */
int picofs_close_file(int fd, bool disable_purge)
{
    int err = -1;
    int i;
    u8_t *erased_area;
    size_t erased_area_size;
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
        custom_fds[fd].cache_trailer.crc = calculate_crc32_universal_unaligned_rtos(custom_fds[fd].cache, custom_fds[fd].data_len);

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

        if (!picofs_find_contiguous_free_area(custom_fds[fd].cache_trailer.file_size, &erased_area, &erased_area_size) && (err == 0))
        {
            // printf("about to copy out of cache into flash\n");
            // hex_dump(custom_fds[fd].cache, h->file_size);
            //memcpy(erased_area, custom_fds[fd].cache, custom_fds[fd].cache_trailer.file_size + padding_len);
            picofs_flash_program(erased_area, custom_fds[fd].cache, custom_fds[fd].cache_trailer.file_size + padding_len);
            err = 0;
        }
        else
        {
            shell_printf("picoFS: out of space writing %s to flash\n",custom_fds[fd].cache_trailer.name);
            err = -2;
        }

        if (!err && !disable_purge)
        {
            picofs_purge_duplicates(custom_fds[fd].cache_trailer.name, custom_fds[fd].cache_trailer.file_id);
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
        if (custom_fds[fd].cache)
        {
            vPortFree(custom_fds[fd].cache);
            custom_fds[fd].cache = NULL;
        }

        // clear the cache        
        custom_fds[fd].cache_len = 0;
        memset(&custom_fds[fd].cache_trailer, 0, sizeof(FILE_TRAILER_T));
    }

    return(err);
}

/*!
 * \brief purge files with duplicate names keeping only one file with the given FID
 * \details Duplicate file names may temporarily occur at an intermediate step of certain operations such as copying over
 *          the top of an existing file.  The original file is not deleted until after the copy is completed.  This function
 *          is used to find and eliminate all duplicates at the completeion of the operation.
 * \param[in]   filename     delete all duplicate files with this name
 * 
 * \param[out]  keep_fid     FID of file to keep
 *  *     
 * \return 0 on success
 */
int picofs_purge_duplicates(char *filename, u8_t keep_fid)
{
    int err = 0;
    int i;
    FILE_TRAILER_T *current = NULL;
    int size_files = 0;

    memset(purge_list, 0, sizeof(purge_list));

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            purge_list[current->file_id].valid = true;

            if (current->file_sequence >= purge_list[current->file_id].trailer->file_sequence) 
            {
                purge_list[current->file_id].trailer = current;
            }
        }
        else
        {
            printf("picofs: error: next iter unexpectedly returned a NULL pointer without an error return value\n");
            err = -1;
            break;
        }
    }

    if (!err)
    {
        for (i=0; i<FS_NUM_FID; i++)
        {
            if ((i != keep_fid) && purge_list[i].valid && !purge_list[i].trailer->file_status && (strcmp(purge_list[i].trailer->name, filename) == 0))
            {
                picofs_printf("%08d\t%d\t%d\t%s ***PURGED***\n", purge_list[i].trailer->file_size, purge_list[i].trailer->file_id, purge_list[i].trailer->file_sequence, purge_list[i].trailer->name);
                picofs_unlink_by_fid(i); 
                size_files += purge_list[i].trailer->file_size;
            }
        }

        if (size_files)
        {
            picofs_printf("Total size of files purged is %d bytes\n", size_files);
        }
    }        

    return(err);
}
