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
int picofs_find_available_fd(void);
int picofs_release_fd(int fd);
int picofs_open_for_deletion(int fd, const char *name, int flags);
int picofs_close_for_deletion(int fd);

// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern FILE_TEST_T test_filesystem[10];
//static variables

 

int picofs_unlink(const char *name) 
{
    int err = -1;
    int i;
    int fd;

    fd = picofs_find_available_fd();   // NB we are bypassing the wrappers so have to manage file descriptors directly in this function
    
    if (fd == -1) 
    {
        errno = ENFILE; // Too many open files
        return -1;
    }

    if (picofs_open_for_deletion(fd, name, O_WRONLY))   
    {
        errno = ENOENT; // File not found
        return -1;
    }

    custom_fds[fd].in_use = true;

    custom_fds[fd].file_status = 1;  // mark for deletion
    custom_fds[fd].data_len = 0;     // empty file
 
    picofs_close(fd);
    
    picofs_release_fd(fd);

    return 0;
}

int picofs_unlink_fid(u8_t fid) 
{
    int err = -1;
    int i;
    int fd;

    fd = picofs_find_available_fd();   // NB we are bypassing the wrappers so have to manage file descriptors directly in this function
    
    if (fd == -1) 
    {
        errno = ENFILE; // Too many open files
        return -1;
    }

    if (picofs_open_for_deletion_by_fid(fd, fid, O_WRONLY))   
    {
        errno = ENOENT; // File not found
        return -1;
    }

    custom_fds[fd].in_use = true;

    custom_fds[fd].file_status = 1;  // mark for deletion
    custom_fds[fd].data_len = 0;     // empty file
 
    picofs_close_for_deletion(fd);
    
    picofs_release_fd(fd);

    return 0;
}

/*!
 * \brief open a file for deletion -- FS_MAX_SEQ is allowed!
 * 
 * \param fd     file descriptor
 * \param name   A pointer to a null-terminated string specifying the path to the file you want to open.
 * \param flags  bitwise flags O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND, O_EXCL 
 * \return 0 on success
 */
int picofs_open_for_deletion(int fd, const char *name, int flags)   // TODO remove duplicate code -- this is nearly identical to picofs_open()
{
    int err = -1;
    FILE_TRAILER_T *file_trailer = NULL;
    char *file_data = NULL;
    int open_mode = 0;


    if (!picofs_find_by_name(name, &file_trailer))
    {      
        // for historical reasons the values 0, 1 and 2 are used for read, write and read/wwrite modes
        // we transform them into more sensible bit flags in the two least significant bits for easier processing
        open_mode = (flags + 1) & (O_ACCMODE);
        MASKED_WRITE(flags, open_mode, O_ACCMODE);
        
        if (flags & FWRITE)
        {
            // need exclusive access for write
            if (!picofs_file_in_use(file_trailer, fd) && (!(flags & O_EXCL)))
            {
                picofs_fd_initialize(fd, flags, (FILE_TRAILER_T *)file_trailer);
                err = picofs_allocate_cache(fd);
                
                if (!err)
                {
                    if (custom_fds[fd].cache_len >= custom_fds[fd].file_len)
                    {
                        // populate cache
                        memcpy(custom_fds[fd].cache, custom_fds[fd].file, custom_fds[fd].file_len);

                        // increment sequence
                        ((FILE_TRAILER_T *)(custom_fds[fd].cache + custom_fds[fd].file_len - sizeof(FILE_TRAILER_T)))->file_sequence++;

                        // reinitialize the file descriptor using the cache
                        picofs_fd_initialize(fd, flags, (FILE_TRAILER_T *)(custom_fds[fd].cache + custom_fds[fd].file_len - sizeof(FILE_TRAILER_T)));

                        // since we are writing to the file set the file descriptor to use the cached trailer
                        custom_fds[fd].file_trailer = &(custom_fds[fd].cache_trailer);
                    }
                    else
                    {
                        picofs_deallocate_cache(fd);
                        err = -3;
                    }
                }
            }            
        }               
    }
    

    return(err);
}

/*!
 * \brief open a file for deletion -- FS_MAX_SEQ is allowed!
 * 
 * \param fd     file descriptor
 * \param name   A pointer to a null-terminated string specifying the path to the file you want to open.
 * \param flags  bitwise flags O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND, O_EXCL 
 * \return 0 on success
 */
int picofs_open_for_deletion_by_fid(int fd, u8_t fid, int flags)   // TODO remove duplicate code -- this is nearly identical to picofs_open()
{
    int err = -1;
    FILE_TRAILER_T *file_trailer = NULL;
    char *file_data = NULL;
    int open_mode = 0;


    if (!picofs_find_by_fid(fid, &file_trailer))
    {      
        // for historical reasons the values 0, 1 and 2 are used for read, write and read/wwrite modes
        // we transform them into more sensible bit flags in the two least significant bits for easier processing
        open_mode = (flags + 1) & (O_ACCMODE);
        MASKED_WRITE(flags, open_mode, O_ACCMODE);
        
        if (flags & FWRITE)
        {
            // need exclusive access for write
            if (!picofs_file_in_use(file_trailer, fd) && (!(flags & O_EXCL)))
            {
                picofs_fd_initialize(fd, flags, (FILE_TRAILER_T *)file_trailer);
                err = picofs_allocate_cache(fd);
                
                if (!err)
                {
                    if (custom_fds[fd].cache_len >= custom_fds[fd].file_len)
                    {
                        // populate cache
                        memcpy(custom_fds[fd].cache, custom_fds[fd].file, custom_fds[fd].file_len);

                        // increment sequence
                        ((FILE_TRAILER_T *)(custom_fds[fd].cache + custom_fds[fd].file_len - sizeof(FILE_TRAILER_T)))->file_sequence++;

                        // reinitialize the file descriptor using the cache
                        picofs_fd_initialize(fd, flags, (FILE_TRAILER_T *)(custom_fds[fd].cache + custom_fds[fd].file_len - sizeof(FILE_TRAILER_T)));

                        // since we are writing to the file set the file descriptor to use the cached trailer
                        custom_fds[fd].file_trailer = &(custom_fds[fd].cache_trailer);
                    }
                    else
                    {
                        picofs_deallocate_cache(fd);
                        err = -3;
                    }
                }
            }            
        }               
    }
    

    return(err);
}

/*!
 * \brief close file 
 *
 * \param fd     file descriptor
 * \return 0 on success
 */
int picofs_close_for_deletion(int fd)   // TODO remove duplicate code -- this is nearly identical to picofs_close() difference is no purge
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

