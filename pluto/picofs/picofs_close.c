/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include <string.h>
 #include <fcntl.h>
#include "FreeRTOS.h"
#include "picofs.h"

//prototypes

// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern int tab_completion_sequence;
#if FAKE_FLASH == 1
    extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

// static variables
static FILE_STATUS_T purge_list[FS_NUM_FID]; 
 
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

    // if mmap is active postpone the close
    if (custom_fds[fd].mmap_ref_count > 0)
    {
        // close will be called by munmap() when the last mapping is released
        custom_fds[fd].mmap_delayed_close = true;

        err = 0;
        return(err);
    }

    // flush cached file data to flash
    err = picofs_sync_file(fd, disable_purge);

    // clear the cache
    picofs_deallocate_cache(fd);

    // clear out the file descriptor
    custom_fds[fd].file = NULL;
    custom_fds[fd].file_len = 0;
    custom_fds[fd].file_trailer = NULL;
    custom_fds[fd].data = NULL;
    custom_fds[fd].data_len = 0;
    custom_fds[fd].data_offset = 0;
    custom_fds[fd].rollover_fid = FS_INVALID_FID;
    custom_fds[fd].mmap_ref_count = 0;
    custom_fds[fd].mmap_delayed_close = false;
    custom_fds[fd].reserved_flash_start = NULL;
    custom_fds[fd].reserved_flash_end = NULL;     

    return(err);
}

/*!
 * \brief sync file to flash
 *
 * \param fd              file descriptor
 * \param disable_purge   do not purge duplicate filenames (only used when already executing a purge)
 * \return 0 on success
 */
int picofs_sync_file(int fd, bool disable_purge)
{
   int err;

    // check if cache contains partial file or the entire file
    if (picofs_cache_contains_entire_file(fd))
    {
        // flush entire file to flash from cache
        err = picofs_sync_file_regular(fd, disable_purge);
    }
    else
    {
        // flash remainder of file from cache
        err = picofs_sync_file_large(fd, disable_purge);  
    }

    return(err);
}

/*!
 * \brief flush file cache to flash --THIS IS USED FOR REGULAR FILES THAT FULLY FIT WITHIN RAM CACHE--
 *
 * \param fd              file descriptor
 * \param disable_purge   do not purge duplicate filenames (only used when already executing a purge)
 * \return 0 on success
 */
int picofs_sync_file_regular(int fd, bool disable_purge)
{
    int err = -1;
    int i;
    uint8_t *erased_area;
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
        custom_fds[fd].cache_trailer.crc = picofs_calculate_crc32(custom_fds[fd].cache, custom_fds[fd].data_len);

        // append trailer to end of cached file 
        if ((custom_fds[fd].data_len + sizeof(FILE_TRAILER_T)) < custom_fds[fd].cache_len)
        {
            memcpy(custom_fds[fd].cache + custom_fds[fd].data_len, &(custom_fds[fd].cache_trailer), sizeof(FILE_TRAILER_T));
            err = 0;
        }
        else
        {
            //TODO: expand cache as needed
            picofs_printf("picoFS: out of cache appending trailer to %s for write to flash\n",custom_fds[fd].cache_trailer.name);
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

        if (!picofs_find_contiguous_free_area(custom_fds[fd].cache_trailer.file_size, &erased_area, &erased_area_size, false) && (err == 0))
        {
            picofs_flash_program(erased_area, custom_fds[fd].cache, custom_fds[fd].cache_trailer.file_size + padding_len);
    
            err = 0;
        }
        else
        {
            picofs_printf("picoFS: out of space writing %s to flash\n",custom_fds[fd].cache_trailer.name);
            err = -2;
        }

        if (!err && !disable_purge)
        {
            picofs_purge_duplicates(custom_fds[fd].cache_trailer.name, custom_fds[fd].cache_trailer.file_id);
        }
    }
    
    // handle pending deletion of the fid used prior to rollover
    if (custom_fds[fd].rollover_fid != FS_INVALID_FID)
    {
        picofs_unlink_by_fid(custom_fds[fd].rollover_fid);
        custom_fds[fd].rollover_fid = FS_INVALID_FID;
    }

    // update global list of files
    picofs_refresh_files();

    tab_completion_sequence++;

    return(err);
}

/*!
 * \brief flush LARGE file cache to flash --THIS IS USED FOR FILES THAT DON'T FIT IN THE RAM CACHE-- 
 *
 * \param fd              file descriptor
 * \param disable_purge   do not purge duplicate filenames (only used when already executing a purge)
 * \return 0 on success
 */
int picofs_sync_file_large(int fd, bool disable_purge)
{
    int err = -1;
    int i;
    uint8_t *erased_area;
    size_t erased_area_size;
    int padding_len = 0;
    int cache_index = 0;
    int saved_cache_index = 0;
    uint32_t ram_crc = 0;
    uint32_t flash_crc = 0;
    uint32_t combined_crc = 0;
    uint32_t final_crc = 0;  
    size_t left_over_trailer = 0;  

    if (!((fd >=0) && (fd < FS_MAX_FILE_DESCRIPTORS)))
    {
        return(err);
    }

    if (!custom_fds[fd].in_use)
    {
        return(err);
    }

    cache_index = custom_fds[fd].data_offset - custom_fds[fd].cache_offset;
    picofs_printf("ci = %0x do = %0x co = %0x\n", cache_index, custom_fds[fd].data_offset, custom_fds[fd].cache_offset);

    if (custom_fds[fd].cache)
    {
        // update trailer

        // set size and status
        custom_fds[fd].cache_trailer.file_size = custom_fds[fd].data_len + sizeof(FILE_TRAILER_T);
        custom_fds[fd].cache_trailer.file_status = custom_fds[fd].file_status;

        // calculate the file CRC in two parts 1) portion of file already in flash 2) remainder of file still in the RAM cache
        flash_crc = picofs_calculate_crc32(custom_fds[fd].reserved_flash_start, custom_fds[fd].cache_offset);
        ram_crc =   picofs_calculate_crc32(custom_fds[fd].cache, cache_index);      

        // calculate the crc of the two blocks concatenated
        combined_crc = picofs_combine_crc32(flash_crc, ram_crc, cache_index);
        custom_fds[fd].cache_trailer.crc = combined_crc; /* ^ 0xFFFFFFFF;*/

        // check if trailer fits entirely in buffer for final data write to flash
        if ((cache_index + sizeof(FILE_TRAILER_T)) < custom_fds[fd].cache_len)
        {
            left_over_trailer = 0;
        }
        else
        {
            left_over_trailer = cache_index + sizeof(FILE_TRAILER_T) - custom_fds[fd].cache_len;
        }

        if (left_over_trailer > sizeof(FILE_TRAILER_T))
        {
            picofs_printf("picofs: Internal error : left_over_trailer is too big!!!!! %0x\n", left_over_trailer);
        }
        
        // copy as much of trailer into cache as will fit
        memcpy(custom_fds[fd].cache + cache_index, &(custom_fds[fd].cache_trailer), (sizeof(FILE_TRAILER_T) - left_over_trailer));

        // compute padding for final data write
        padding_len = left_over_trailer?0:(cache_index + sizeof(FILE_TRAILER_T))%256?(256 - (cache_index + sizeof(FILE_TRAILER_T))%256):0;
        picofs_printf("padding length = %0x left_over_trailer = %0x\n", padding_len, left_over_trailer);
        picofs_printf("writing FINAL pages to flash @ %0x with size = %0x [datalen = %0x]\n", custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset, cache_index + sizeof(FILE_TRAILER_T) - left_over_trailer + padding_len, cache_index);
        picofs_flash_program(custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset, custom_fds[fd].cache, cache_index + sizeof(FILE_TRAILER_T) - left_over_trailer + padding_len);

        if (!left_over_trailer)
        {
            // final page writen
            err = 0;
        }
        else
        {
            // additional page required to write remainder of the trailer
            
            // clear the cache 
            memset(custom_fds[fd].cache, FS_ERASED_CELL_VALUE, custom_fds[fd].cache_len);

            // shift the cache window 
            custom_fds[fd].cache_offset += custom_fds[fd].cache_len;

            // copy remainder of trailer into cache
            memcpy(custom_fds[fd].cache, ((char *)&(custom_fds[fd].cache_trailer))+(sizeof(FILE_TRAILER_T) - left_over_trailer), (sizeof(FILE_TRAILER_T) - left_over_trailer));

            picofs_printf("writing LEFT OVER trailer single page to flash @ %d with size = %d\n", custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset, FS_PAGE_SIZE);
            picofs_flash_program(custom_fds[fd].reserved_flash_start + custom_fds[fd].cache_offset, custom_fds[fd].cache, FS_PAGE_SIZE);            
        }

        if (!err && !disable_purge)
        {
            picofs_purge_duplicates(custom_fds[fd].cache_trailer.name, custom_fds[fd].cache_trailer.file_id);
        }  
    }
    
    // handle pending deletion of the fid used prior to rollover
    if (custom_fds[fd].rollover_fid != FS_INVALID_FID)
    {
        picofs_unlink_by_fid(custom_fds[fd].rollover_fid);
        custom_fds[fd].rollover_fid = FS_INVALID_FID;
    }

    // update global list of files
    picofs_refresh_files();

    tab_completion_sequence++;


    final_crc = picofs_calculate_crc32(custom_fds[fd].reserved_flash_start, custom_fds[fd].data_len);

    picofs_printf("FINAL_DATA_LEN = %d\n\nFILE CRC SUMMARY\nflash=%0x\nram=%0x\ncombined=%0x\nfinal=%0x\n", custom_fds[fd].data_len, flash_crc, ram_crc, combined_crc, final_crc);

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
        custom_fds[fd].cache_offset = 0;
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
int picofs_purge_duplicates(char *filename, uint8_t keep_fid)
{
    int err = 0;
    int i;
    FILE_TRAILER_T *current = NULL;
    int size_files = 0;

    memset(purge_list, 0, sizeof(purge_list));

    while(!picofs_iter_next_file(&current, false))
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
            picofs_printf("picofs: error: next iter unexpectedly returned a NULL pointer without an error return value\n");
            err = -1;
            break;
        }
    }

    if (!err)
    {
        for (i=0; i<FS_NUM_FID; i++)
        {
            if ((i != keep_fid) && purge_list[i].valid && !(purge_list[i].trailer->file_status & STS_DELETED) && (strcmp(purge_list[i].trailer->name, filename) == 0))
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


