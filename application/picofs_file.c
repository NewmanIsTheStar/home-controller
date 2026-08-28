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



// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif


//static variables
FILE_STATUS_T picofs_files[FS_NUM_FID]; 


/*!
 * \brief check if sequence number is the latest for given file name by scanning flash
 * 
 * \param[in]   filename     name to find
 * \param[in]   file_id      fid to find
 * 
 * \param[out]  sequence       pointer to file header
 *  *     
 * \return 0 if latest, 1 if not latest sequence
 */
int picofs_is_latest_file_sequence_from_flash(char *filename, u8_t file_id, u8_t sequence) 
{
    int islatest = 1;
    FILE_TRAILER_T *t = NULL;

    while(!picofs_iter_next_file(&t, false))
    {
        if (t)
        {
            if (((t->file_id == file_id) || (strcmp(t->name, filename) == 0)) && (t->file_sequence > sequence))
            {
               islatest = 0;  // we found a later sequence
               break;
            }
        }
        else
        {
            printf("picofs: error: next iter unexpectedly returned a NULL pointer without an error return value\n");
            break;
        }
    }

    return(islatest);
}

/*!
 * \brief check if sequence number is the latest for given file name using cache
 * 
 * \param[in]   filename     name to find
 * \param[in]   file_id      fid to find
 * \param[out]  sequence       pointer to file header
 *  *     
 * \return 0 if latest, 1 if not latest sequence
 */
int picofs_is_latest_file_sequence_from_cache(char *filename, u8_t file_id, u8_t sequence)
{
    int islatest = 1;
    int i;
    
    // use fid if provided to index directly
    if (file_id != FS_INVALID_FID)
    {
        if (picofs_files[i].trailer->file_sequence > sequence)
        {
            islatest = 0;
        }
    }
    else // search for name
    {
        for(i=0; i< FS_NUM_FID; i++)
        {
            if ((strcmp(picofs_files[i].trailer->name, filename) == 0) && (picofs_files[i].trailer->file_sequence > sequence))
            {
                islatest = 0;
                break;
            }
        }
    }

    return(islatest);
}


/*!
 * \brief Create initial trailer for new empty file
 * 
 * \param[in]   fd     file descriptor
 * 
 * \param[out]  name   file name
 *  
 * \return 0 on success
 */
int picofs_create_file_trailer(int fd, const char *name)
{
    int err = -1;
    u8_t file_id = FS_INVALID_FID;

    file_id = picofs_get_new_file_id();

    // if file_id is valid
    if ((file_id != FS_INVALID_FID))
    {       
        // create new trailer in the fd cache 
        STRNCPY(custom_fds[fd].cache_trailer.magic_number, "pfs", sizeof(custom_fds[fd].cache_trailer.magic_number));                                                            
        custom_fds[fd].cache_trailer.picofs_version = FS_VERION;
        custom_fds[fd].cache_trailer.file_id = file_id;
        custom_fds[fd].cache_trailer.file_sequence = 0;
        custom_fds[fd].cache_trailer.file_status= 0;
        custom_fds[fd].cache_trailer.file_size = sizeof(FILE_TRAILER_T);
        custom_fds[fd].cache_trailer.crc = 0;
        STRNCPY(custom_fds[fd].cache_trailer.name, name, sizeof(custom_fds[fd].cache_trailer.name));

        err = 0;
    }

    // store trailer in the cached file too (this is used to bootstrap the second initialization of the fd from cache)
    if (!err && custom_fds[fd].cache)
    {        
        memcpy(custom_fds[fd].cache, (char *)&(custom_fds[fd].cache_trailer), sizeof(FILE_TRAILER_T));
    }    

    return(err);
}


/*!
 * \brief get a new file id
 * 
 * \return file_id or FS_INVALID_FID on failure
 */
u8_t picofs_get_new_file_id(void)
{
    int i;
    int fid = FS_INVALID_FID;

    for(i=0; i<FS_NUM_FID; i++)
    {
        if (!picofs_files[i].valid)
        {
            // TODO: locking and claim fid as reserved but not valid yet
            fid = i;
            break;
        }
    }

    return(fid);
}



/*!
 * \brief find file id and sequence for file at given address
 * 
 * \return file_id or FS_INVALID_FID on failure
 */
u8_t picofs_find_file_at_location(char *search, FILE_TRAILER_T **trailer)
{
    int err = -1;
    char *p = NULL;
    FILE_TRAILER_T *t = NULL;
    u8_t file_id = FS_INVALID_FID;
    u8_t file_sequence = 0;    
    bool found = false;

    *trailer = NULL;

    if ((search < FS_START) || (search >= FS_END))
    {
        // search location is invalid
        return (FS_INVALID_FID);
    }

    // scan forwards through flash
    p = search;

    // scan flash
    while (p < FS_END)
    {
        t = (FILE_TRAILER_T *)p;

        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION))
        {
            // printf("Search began @ %p  found first file trailer @ %p which is a delta of %p\n", search, p, p-search);
            // check that file trailer is for a file that overlaps our search location 
            if ((char *)(p - t->file_size) <= search)
            {
                file_id = t->file_id;
                file_sequence = t->file_sequence;
                *trailer = t;               
            }
            else
            {
                // printf("ignoring file %s because its size %0x does not overlap the search start point\n", t->name, t->file_size);
                hex_dump(search, p-search+sizeof(FILE_TRAILER_T));
            }

            break;
        }
        else
        {
            p++;
        }
    }

    return(file_id);
}

/*!
 * \brief iterator function to move to the next file in flash searching backwards
 * \param[in]   current_file      pointer to current file or NULL to initiate new walk
 * 
 * \return 0 on success
 */
int picofs_iter_next_file(FILE_TRAILER_T **current_file, bool ignore_crc)
{
    int not_found = 1;
    char *p = NULL;
    FILE_TRAILER_T *t = NULL;

    //ignore_crc = true;

    if (((char *)*current_file < FS_START) || ((char *)*current_file >= FS_END))
    {
        p = FS_END - 1 - sizeof(FILE_TRAILER_T);
    }
    else
    {
        p = (char *)*current_file;
    }

    // scan backwards through flash until we find next file
    do
    {
        t = (FILE_TRAILER_T *)p;

        // move to new location
        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION) &&
            (t->file_size >= sizeof(FILE_TRAILER_T)) &&
            (ignore_crc || (t->crc == picofs_calculate_crc32(((const uint8_t *)(p + sizeof(FILE_TRAILER_T) - t->file_size)), t->file_size - sizeof(FILE_TRAILER_T)))))
        {
            p = p - t->file_size;  
        }        
        else
        {
            p--;
        }

        // check if new location contains a file trailer
        t = (FILE_TRAILER_T *)p;
        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION) &&
            (t->file_size >= sizeof(FILE_TRAILER_T)) &&
            (ignore_crc || (t->crc == picofs_calculate_crc32(((const uint8_t *)(p + sizeof(FILE_TRAILER_T) - t->file_size)), t->file_size - sizeof(FILE_TRAILER_T)))))            
        {
            not_found = 0;
            break;
        }

    } while ((p >= FS_START) && not_found);
    
    if (!not_found)
    {
        *current_file = (FILE_TRAILER_T *)p;

        // printf("Iterator return: %s %d %p\n", t->name, not_found, *current_file);
    }
    else
    {
        // printf("Iterator returning not found %d\n", not_found);
    }

    return(not_found);
}





/*!
 * \brief rebuild the global list of files
 *     
 * \return 0 on success
 */
int picofs_refresh_files(void) 
{
    int err = -1;
    FILE_TRAILER_T *current = NULL;

    memset(picofs_files, 0, sizeof(picofs_files));

    while(!picofs_iter_next_file(&current, false))
    {
        if (current)
        {
            picofs_files[current->file_id].valid = true;

            if ((current->file_sequence >= picofs_files[current->file_id].trailer->file_sequence))
            {
                picofs_files[current->file_id].trailer = current;
            }
        }
        else
        {
            printf("picofs: error: next iter unexpectedly returned a NULL pointer without an error return value\n");
            break;
        }
    }

    return(0);
}



/*!
 * \brief increment sequence number and handle rollover
 * 
 * \param[in]   trailer     trailer to modify
 *     
 * \return 0 on success
 */
int picofs_increment_sequence(FILE_TRAILER_T *trailer)
{
    int err = -1;
    u8_t new_fid = FS_INVALID_FID;

    if (trailer->file_sequence == (FS_MAX_SEQ -1))
    {
        // out of sequence numbers so change to new FID NB: caller is responsible for deleting the old fid
        new_fid = picofs_get_new_file_id();

        if (new_fid != FS_INVALID_FID)
        {
            trailer->file_id = new_fid;
            trailer->file_sequence = 0;

            err = 0;
        }
    }
    else
    {
        trailer->file_sequence++;
        err = 0; 
    }   

    return(err);
}



/*!
 * \brief check if deleted file has remnants in other sectors (this means it cannot be erased)
 * 
 * \param[in]   deleted_file     deleted file trailer
 * 
 * \return 0 on success
 */
bool picofs_deleted_file_has_remnants_in_other_sectors(FILE_TRAILER_T *deleted_file) 
{
    bool has_remnants = false;
    int i;
    FILE_TRAILER_T *current = NULL;
    int deleted_file_start_sector = 0;
    int deleted_file_end_sector = 0;

    deleted_file_start_sector = picofs_get_start_sector(deleted_file);
    deleted_file_end_sector = picofs_get_end_sector(deleted_file);

    if (deleted_file_start_sector != deleted_file_end_sector)
    {
        printf("picofs: error: deleted file spans sectors %d to %d\n", deleted_file_start_sector, deleted_file_end_sector);
        return(true);
    }

    while(!picofs_iter_next_file(&current, false))
    {
        if (current)
        {
            if (current->file_id == deleted_file->file_id)
            {
                if ((deleted_file_start_sector != picofs_get_start_sector(current)) || 
                    (deleted_file_start_sector != picofs_get_end_sector(current)))
                {
                    has_remnants = true;
                    break;
                }
            }
        }
        else
        {
            printf("picofs: error: next iter unexpectedly returned a NULL pointer without an error return value\n");
            break;
        }
    }

    return(has_remnants);
}

/*!
 * \brief check if file is deleted and can erased
 * 
 * \param[in]   candidate_file     candidate file trailer
 *     
 * \return 0 on success
 */
bool picofs_deleted_file_ready_for_erasure(FILE_TRAILER_T *candidate_file)
{
    bool may_erase = false;

    // check if file is deleted
    if (candidate_file && candidate_file->file_status == 1)
    {        
        may_erase = !picofs_deleted_file_has_remnants_in_other_sectors(candidate_file);
    }

    return(may_erase);
}
