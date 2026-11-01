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

    if (!known_size && !(flags & O_TRUNC))
    {
        // use existing file size
        //known_size = ((custom_fds[fd].file_len + (4*1024))/(4*1024))*(4*1024);
        known_size = custom_fds[fd].file_len;
    }
    
    // initialize flash reservation
    custom_fds[fd].reserved_flash_start = NULL;
    custom_fds[fd].reserved_flash_end = NULL;

    // allocate cache
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
        //cache_size = custom_fds[fd].cache_len;                
                
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
    int bytes_of_cache_to_flash = 0;

    /* Plan of attack
       1. make fd check return at top to descrease indentation
       2. reject resize if > 16K and cache not NULL
       3. implement new function to transition from all-in-cache to partial-in-cache
       4. call new function from write function
       5. make sure data_offset is known to new function (currently in variable i and not copied to datat_offset until end of write)*/
       
    // check file descriptor is valid
    if ((fd <0) && (fd >= FS_MAX_FILE_DESCRIPTORS) )
    {
        return(-1);
    }

    // check for attempt to resize a large file 
    if (/*custom_fds[fd].cache_len > 0) &&*/ !picofs_cache_contains_entire_file(fd))
    {
        // reject the resize
        picofs_printf("picofs: ERROR attempted to expand the size of a large file. You must must truncate and overwrite the entire file.\n");
        return(-3);
    }

    // round up requested size to the nearest sector (4k)
    new_cache_size = ((requested_size + (4*1024))/(4*1024))*(4*1024); 

    if (new_cache_size > FS_FILE_CACHE_MAX)
    {
        if (custom_fds[fd].cache && picofs_cache_contains_entire_file(fd))
        {
            picofs_printf("picofs: WARNING attempted to expand normal file cache beyond %d byte.\n", FS_FILE_CACHE_MAX);
            return(-2);
        }
        
        // attemot to make large file flash reservation
        flash_reservation_size = (((new_cache_size  + sizeof(FILE_TRAILER_T) + FS_PAGE_SIZE)/FS_PAGE_SIZE))*FS_PAGE_SIZE; 

        // set minimal cache size
        new_cache_size = FS_FILE_CACHE_MIN;

        // reserve flash and make 64K aligned [for now we assume all large files are executable but this should be a passed parameter in future]
        if (!picofs_find_contiguous_free_area(flash_reservation_size, &(custom_fds[fd].reserved_flash_start), &actual_reservation_size, true))
        {
            custom_fds[fd].reserved_flash_end = custom_fds[fd].reserved_flash_start + flash_reservation_size;
            custom_fds[fd].cache_offset = 0;

            picofs_printf("picofs_cache_realloc:RESERVED FLASH: size = %d start = %08x end = %08x filename = %s\n", flash_reservation_size, custom_fds[fd].reserved_flash_start, custom_fds[fd].reserved_flash_end, custom_fds[fd].cache_trailer.name);            

            // TODO: pass explicit parameter or set this bit externally for EXE files -- at present we presume large files (>16KB) are EXE
            custom_fds[fd].file_status |= STS_EXECUTABLE;
        }
        else
        {
            picofs_printf("picofs_cache_realloc: failed to find a contiguous area of flash with size %0x\n", flash_reservation_size);
            new_cache_size = 0;
            err = -4;
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

/*!
 * \brief convert open file from fully cached to partially cached large file
 *
 * \param fd     file descriptor
 * \return true if entire file fits inside the RAM cache
 */
int picofs_cache_convert_to_large_file(int fd, size_t offset, size_t flash_reservation_size, PFS_ALIGNMENT_TYPE_T flash_alignment)
{
    char *new_cache = NULL;    
    size_t actual_reservation_size = 0;

    // transition from fully cached file to parially cached (one time transition)  TODO: limit this to append mode
    picofs_printf("picofs: transitioning file from fully cached to partially cached\n");

    if (offset%FS_PAGE_SIZE)
    {
        picofs_printf("picofs: error: conversion to large file at non-page boundary. aborted.\n");
        return(-1);
    }

    if (offset%FS_SECTOR_SIZE)
    {
        picofs_printf("picofs: error: conversion to large file at non-sector boundary. aborted.\n");  // this presumes that cache is allocated in sectors
        return(-2);
    }    

    if (offset != custom_fds[fd].cache_len)
    {
        picofs_printf("picofs: error: conversion to large file when cache is not neither full nor empty. aborted.\n");  
        return(-3);        
    }

    // reserve flash
    if (!picofs_find_contiguous_free_area(flash_reservation_size, &(custom_fds[fd].reserved_flash_start), &actual_reservation_size, flash_alignment))
    {
        custom_fds[fd].reserved_flash_end = custom_fds[fd].reserved_flash_start + flash_reservation_size;
        custom_fds[fd].cache_offset = 0;

        picofs_printf("picofs_cache_convert_to_large_file:RESERVED FLASH: size = %d start = %08x end = %08x filename = %s\n", flash_reservation_size, custom_fds[fd].reserved_flash_start, custom_fds[fd].reserved_flash_end, custom_fds[fd].cache_trailer.name);
        if (flash_alignment == PFS_ALIGN_BLOCK)
        {
            custom_fds[fd].file_status |= STS_EXECUTABLE;
        }
    }
    else
    {
        picofs_printf("picofs_cache_convert_to_large_file: failed to find a contiguous area of flash with size = %0x\n", flash_reservation_size);
        return(-4);
    }

    if (offset)
    {
        // write the entire cache out to flash
        picofs_flash_program(custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset, custom_fds[fd].cache, offset);
    }
    
    // shift the cache window 
    custom_fds[fd].cache_offset = offset;
     
    // allocate new minimal cache
    new_cache = pvPortMalloc(FS_FILE_CACHE_MIN);    

    if (new_cache && custom_fds[fd].cache)
    {
        // delete old cache
        vPortFree(custom_fds[fd].cache);
        custom_fds[fd].cache = NULL;          
    }

    if (new_cache)
    {
        // set file descriptor to use the new cache
        custom_fds[fd].cache = new_cache;
        custom_fds[fd].cache_len = FS_FILE_CACHE_MIN; 
        custom_fds[fd].data = new_cache;

        // clear the new cache
        memset(custom_fds[fd].cache, FS_ERASED_CELL_VALUE, custom_fds[fd].cache_len);       
    }

    return(0);
}