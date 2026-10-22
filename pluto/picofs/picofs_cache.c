/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fcntl.h> 
#include <string.h>
#include "FreeRTOS.h"
#include "picofs.h"





//prototypes
int picofs_fd_new(int fd, int flags, char *name);
int picofs_find_file_in_flash(const char *filename, uint8_t fid, FILE_TRAILER_T **trailer);

// external variables
// extern uint32_t unix_time;
// extern APP_CONFIG_T config;
// extern SYS_WEB_VARIABLES_T sys_web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FS_FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif
extern FILE_STATUS_T picofs_files[FS_NUM_FID]; 

// global variable


//static variables


/*!
 * \brief allocate RAM cache for file writes
 *
 * \param fd     file descriptor
 * \return nothing
 */
int picofs_cache_allocate(int fd, int flags, size_t known_size)
{
    int err = -1;

    if (!known_size)
    {
        // use existing file size
        known_size = ((custom_fds[fd].file_len + (4*1024))/(4*1024))*(4*1024);
    }
    
    err =  picofs_cache_realloc(fd, known_size, FS_ERASED_CELL_VALUE);

    return(err);
}

/*!
 * \brief expand cache by one sector
 *
 * \param fd     file descriptor
 * \return nothing
 */
int picofs_cache_expand(int fd)
{
    int err = -1;
    size_t cache_size = 0;
    char *expanded_cache = NULL;

    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS) )
    {
        // allocate one 4k sector greater than currently used
        cache_size = ((custom_fds[fd].cache_len + (4*1024))/(4*1024))*(4*1024);        
                
        err = picofs_cache_realloc(fd, cache_size, 0xFF);        
    }

    return(err);
}


/*!
 * \brief expand cache by one sector
 *
 * \param fd     file descriptor
 * \return nothing
 */
int picofs_cache_realloc(int fd, size_t requested_size, uint8_t fill)
{
    int err = -1;
    size_t new_cache_size = 0;
    char *new_cache = NULL;
    size_t flash_reservation_size = 0;
    size_t actual_reservation_size = 0;   // the contiguous erased area may be larger than requested


    if ((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS) )
    {
        // round up requested size to the nearest sector (4k)
        new_cache_size = ((requested_size + (4*1024) - 1)/(4*1024))*(4*1024); 
        //picofs_printf("picofs_cache_realloc: requested_size = %0x new_cache_size = %0x\n",requested_size, new_cache_size);

        if (new_cache_size >= FS_FILE_CACHE_MAX)
        {
            // large file flash reservation
            flash_reservation_size = (((new_cache_size  + sizeof(FILE_TRAILER_T) + FS_PAGE_SIZE)/FS_PAGE_SIZE))*FS_PAGE_SIZE; 

            // set minimal cache size
            new_cache_size = FS_FILE_CACHE_MIN;

            // check for attempt to resize flash reservation
            // the only supported use case for writing to existing large files is to overwrite them entirely (O_TRUNC)
            if (!picofs_cache_contains_entire_file)
            {
                picofs_printf("picofs_cache_realloc: WARNING resizing the flash reservation is unsupported.  Previously written flash sectors will NOT be copied into the new reservation.\n");
                err = -2;
            }

            // reserve flash and make 64K aligned [for now we assume all large files are executable but this should be a passed parameter in future]
            if (!picofs_find_contiguous_free_area(flash_reservation_size, &(custom_fds[fd].reserved_flash_start), &actual_reservation_size, true))
            {
                custom_fds[fd].reserved_flash_end = custom_fds[fd].reserved_flash_start + flash_reservation_size;
                custom_fds[fd].cache_offset = 0;

                // TODO: pass explicit parameter or set this bit externally for EXE files -- at present we presume large files (>16KB) are EXE
                custom_fds[fd].file_status |= STS_EXECUTABLE;

                //picofs_printf("flash reservation: ask = %0x actual = %0x\n", flash_reservation_size, actual_reservation_size);
            }
            else
            {
                picofs_printf("picofs_allocate_cache: failed to find a contiguous area of flash for new_cache_size = %0x\n", new_cache_size);
                new_cache_size = 0;
                err = -3;
            }

        }      

        // adjust cache size if necessary
        if (new_cache_size && (new_cache_size != custom_fds[fd].cache_len))
        {            
            // allocate cache
            new_cache = pvPortMalloc(new_cache_size);

            if (new_cache && custom_fds[fd].cache)
            {
                if (new_cache_size > custom_fds[fd].data_len)
                {
                    // copy all data 
                    memcpy(new_cache, custom_fds[fd].cache, custom_fds[fd].data_len);

                    // fill remainder of cache
                    memset(new_cache + custom_fds[fd].data_len, fill, new_cache_size - custom_fds[fd].data_len);
                }
                else
                {
                    // copy truncated data
                    memcpy(new_cache, custom_fds[fd].cache, new_cache_size);
                }

                // delete original cache
                vPortFree(custom_fds[fd].cache);
                custom_fds[fd].cache = NULL;
            }
            else if (new_cache)
            {
                // no previous cache to copy from so fill the newly created cache
                memset(new_cache, fill, new_cache_size);
            }  
            
            if (new_cache)
            {
                // point file descriptor to the new new cache
                custom_fds[fd].cache = new_cache;
                custom_fds[fd].cache_len = new_cache_size;
                custom_fds[fd].data = new_cache;

                if (picofs_cache_contains_entire_file(fd))
                {
                    CLIP(custom_fds[fd].data_len, 0, custom_fds[fd].cache_len);
                }

                //picofs_printf("picofs_cache_realloc: fd = %d new cache = %p [cache size %0x]\n", fd, custom_fds[fd].cache, custom_fds[fd].cache_len);
                err = 0;
            }            
        }     
    }

    return(err);
}


/*!
 * \brief check if cache holds entire file
 *
 * \param fd     file descriptor
 * \return true if entire file fits inside the RAM cache
 */
inline bool picofs_cache_contains_entire_file(int fd)
{
    bool entire_file_cached = true;

    if (custom_fds[fd].reserved_flash_start && custom_fds[fd].reserved_flash_end)
    {
        entire_file_cached = false;
    }

    return(entire_file_cached);
}


