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



/*

*/


/*!
 * \brief open a file
 * 
 * The access mode must include exactly one of the following core options:
 * O_RDONLY: Open for reading only.
 * O_WRONLY: Open for writing only.
 * O_RDWR: Open for both reading and writing.
 * 
 * You can bitwise OR (|) these with additional file creation or status flags:
 * O_CREAT: Create the file if it does not exist.
 * O_TRUNC: Truncate the file length to 0 if it already exists and is opened for writing.
 * O_APPEND: Move the file offset pointer to the end of the file before every write.
 * O_EXCL: Ensure that this call creates the file; if the file already exists, the call fails (used alongside O_CREAT).
 * 
 * \param fd                    file descriptor
 * \param name                  A pointer to a null-terminated string specifying the path to the file you want to open.
 * \param flags                 bitwise flags O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_TRUNC, O_APPEND, O_EXCL 
 * \param fid                   file identifier to use instead of text name 
 * \param disable_fid_rollover  allow file with maximum sequence number to be opened (this is only used for deletion of the file) 
 * \return 0 on success
 */
int picofs_open_file(int fd, const char *name, int flags, u8_t fid, bool disable_fid_rollover)
{
    int err = -1;
    FILE_TRAILER_T *file_trailer = NULL;
    char *file_data = NULL;
    int open_mode = 0;

    // hex_dump((char *)test_filesystem, 512);

    if (!picofs_find_file(name, fid, &file_trailer))
    {      
        // for historical reasons the values 0, 1 and 2 are used for read, write and read/wwrite modes
        // we transform them into more sensible bit flags in the two least significant bits for easier processing
        printf("Open called with flags = %d\n", flags);
        open_mode = (flags + 1) & (O_ACCMODE);
        printf("open_mode = %d\n", open_mode);
        MASKED_WRITE(flags, open_mode, O_ACCMODE);
        printf("updated flags = %d\n", flags);
        
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
                        //custom_fds[fd].cache_trailer.file_sequence++;  don't do this we are about to overwrite this 

                        // reinitialize the file descriptor using the cache
                        picofs_fd_initialize(fd, flags, (FILE_TRAILER_T *)(custom_fds[fd].cache + custom_fds[fd].file_len - sizeof(FILE_TRAILER_T)));

                        // since we are writing to the file set the file descriptor to use the cached trailer
                        custom_fds[fd].file_trailer = &(custom_fds[fd].cache_trailer);

                        if (!disable_fid_rollover && (custom_fds[fd].file_trailer->file_sequence == FS_MAX_SEQ))
                        {
                            // out of sequence numbers so change to new FID and delete old file
                            custom_fds[fd].file_trailer->file_id = picofs_get_new_file_id();
                            custom_fds[fd].file_trailer->file_sequence = 0;

                            if (custom_fds[fd].file_trailer->file_id == FS_INVALID_FID)
                            {
                                printf("picoFS: out of file identifiers\n");
                                picofs_deallocate_cache(fd);
                                err = -6;
                            }
                            else
                            {
                                picofs_unlink_by_name(custom_fds[fd].file_trailer->name); 
                            }
                        }
                    }
                    else
                    {
                        picofs_deallocate_cache(fd);
                        err = -3;
                    }
                }
            
                if (!err && (flags & O_APPEND))
                {
                    printf("APPEND flag\n");
                    custom_fds[fd].data_offset = custom_fds[fd].data_len;
                }

                if (!err && (flags & O_TRUNC))
                {
                    // truncate file
                    custom_fds[fd].data_offset = 0;
                    custom_fds[fd].file_len = sizeof(FILE_TRAILER_T);
                    custom_fds[fd].data_len = 0;

                    // update file trailer in cache  TODO: why not directly access cached trailer in the fd
                    custom_fds[fd].file_trailer->file_size = custom_fds[fd].file_len;
                }
            }            
        }  
        else if (flags & FREAD)
        {
            err = 0;

            picofs_fd_initialize(fd, flags, (FILE_TRAILER_T *)file_trailer);
        }               
    }
    else
    {
        printf("FILE DOES NOT EXIST -- CREATING NEW FILE\n");
        // file does not exit
        if (((flags & O_WRONLY) || (flags & O_RDWR)) && (flags & O_CREAT))
        {
            // create empty file descriptor
            picofs_fd_initialize(fd, flags, NULL);

            // allocate cache for file descriptor
            err = picofs_allocate_cache(fd);

            
            if(!err)  
            {
                // create file trailer in cache
                err = picofs_create_file_trailer(fd, name);

                if (!err)
                {
                    // reininitialize the file descriptor using the file trailer
                    //picofs_fd_initialize(fd, (FILE_TRAILER_T *)custom_fds[fd].cache);
                    //picofs_fd_initialize(fd, (FILE_TRAILER_T *)(custom_fds[fd].cache + custom_fds[fd].file_len - sizeof(FILE_TRAILER_T)));

                    printf("COMPARISON ::  %d should equal %d\n", custom_fds[fd].file_len, sizeof(FILE_TRAILER_T));
                    picofs_fd_initialize(fd, flags, (FILE_TRAILER_T *)(custom_fds[fd].cache));  // empty file only contains trailer
                }
                else
                {
                    // failed to create trailer in the cache so free the cache memory
                    picofs_deallocate_cache(fd);
                }
                
            }
        }
    }

    printf("At completion of picofs_open err %d id %d sq %d sz %d st %d\n", err, custom_fds[fd].file_trailer->file_id, custom_fds[fd].file_trailer->file_sequence, custom_fds[fd].file_trailer->file_size, custom_fds[fd].file_trailer->file_status);
    return(err);
}

/*!
 * \brief allocate RAM cache for file writes
 *
 * \param fd     file descriptor
 * \param header file header
 * \return nothing
 */
int picofs_fd_initialize(int fd, int flags, FILE_TRAILER_T *trailer)
{
    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {
        // retain flags passed to open
        custom_fds[fd].flags = flags;

        if (trailer)
        {
            // cache the trailer in case we write data to the file -- this will overwrite the original trailer
            memcpy(&(custom_fds[fd].cache_trailer), (char *)trailer, sizeof(FILE_TRAILER_T));

            // initialize the file descriptor from the trailer
            custom_fds[fd].file = (char *)trailer + sizeof(FILE_TRAILER_T) - trailer->file_size;
            
            custom_fds[fd].file_len = trailer->file_size;
            custom_fds[fd].file_status = trailer->file_status;
            custom_fds[fd].file_trailer = trailer;
            custom_fds[fd].data = custom_fds[fd].file;
            if (custom_fds[fd].file_len >= (sizeof(FILE_TRAILER_T)))
            {
                custom_fds[fd].data_len = custom_fds[fd].file_len - sizeof(FILE_TRAILER_T);
            }
            else
            {
                printf("picoFS: file data length less that zero -- data loss may have occured len = %d vs expected = %d\n", custom_fds[fd].file_len, sizeof(FILE_TRAILER_T));
                custom_fds[fd].data_len = 0;
            }
            custom_fds[fd].data_offset = 0;
        }
        else
        {
            custom_fds[fd].file = NULL;
            custom_fds[fd].file_len = 0;
            custom_fds[fd].file_status = 0;
            custom_fds[fd].file_trailer = NULL;
            custom_fds[fd].data = NULL;
            custom_fds[fd].data_len = 0;
            custom_fds[fd].data_offset = 0;

            // NB we rely on the cache not being touched here during double initialization sequences
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
        // clean up -- this should never happen !!! TODO: remove as this is potentially worse than leaking memory as it could corupt the heap
        if (custom_fds[fd].cache)
        {
            printf("Hanging cache allocation discovered and cleaned up\n");
            vPortFree(custom_fds[fd].cache);
            custom_fds[fd].cache = NULL;
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
 * \param file_trailer pointer to file trailer
 * \param held_fid  fid of file that caller is trying to open exclusively 
 * \return nothing
 */
bool picofs_file_in_use(FILE_TRAILER_T *file_trailer, int held_fid)
{
    int i;
    bool in_use = false;

    for(i=0; i < FS_MAX_FILE_DESCRIPTORS; i++)
    {
        if (i != held_fid)  // don't contend with ourself!
        {
            if (file_trailer && (((FILE_TRAILER_T *)file_trailer)->file_id == custom_fds[i].file_trailer->file_id) && custom_fds[i].in_use)        
            {
                in_use= true;
                break;
            }
        }
    }

    // printf("IN USE = %d\n", in_use);
    return(in_use);
}

/*!
 * \brief Get pointer to file header matching either the passed filename or fid.  As fid is unique it preempts name if given.
 * 
 * \param[in]   filename     name to find
 * \param[in]   fid          fid to find or FS_INVALID_FID, if valid the fid is used instead of the name
 * \param[out]  trailer      pointer to file trailer
 * \return 0 on success
 */
int picofs_find_file(const char *filename, u8_t fid, FILE_TRAILER_T **trailer)
{
    int err = -1;
    int i;
    u8_t *p = NULL;
    FILE_TRAILER_T *t = NULL;
    u8_t best_sequence = 0;
    u8_t best_status = 0;
    bool first_sequnce = true;

    // scan backwards through flash
    p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash
    while (((char *)p) >= FS_FLASH_START)
    {
        t = (FILE_TRAILER_T *)p;

        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION) &&
            (((fid == FS_INVALID_FID) && (strcmp(t->name, filename) == 0)) || ((fid != FS_INVALID_FID) && (t->file_id == fid))) &&
            !picofs_is_file_deleted(t->file_id))
        {
            // match
            if (first_sequnce)
            {
                best_sequence = t->file_sequence;
                best_status = t->file_status;
                *trailer = t;
                err = 0; 
                p = p - t->file_size; 

                first_sequnce = false;
            }
            else
            {
                if (t->file_sequence > best_sequence) 
                {
                    best_sequence = t->file_sequence;
                    best_status = t->file_status;
                    *trailer = t;
                    err = 0;
                    p = p - t->file_size;                  
                }
            }
        }

        if (p == ((u8_t *)t))
        {
            p--;
        }
    }


    if (best_status) // file was deleted
    {
        err = -1;
    }

    return(err);
}


/*!
 * \brief check if file has been deleted
 * 
 * \param[in]   file_id      file to check
 * \return true if deleted
 */
bool picofs_is_file_deleted(u8_t file_id)
{
    int err = -1;
    int i;
    u8_t *p = NULL;
    FILE_TRAILER_T *t = NULL;
    bool deleted = false;


    // scan backwards through flash
    p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash
    while (((char *)p) >= FS_FLASH_START)
    {
        t = (FILE_TRAILER_T *)p;

        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION) &&
            (t->file_id == file_id) && 
            (t->file_status))
        {
            deleted = true;
            break;
        }

        if (p == ((u8_t *)t))
        {
            p--;
        }
    }

    return(deleted);
}

int picofs_initialize(void)
{
    int err = -1;
    static bool init_fds = true;

    // zero out all file descriptiors before using file system 
    if (init_fds)
    {
        memset((char *)custom_fds, 0, sizeof(custom_fds));
        init_fds = false;
        err = 0;
    }

    return(err);
}