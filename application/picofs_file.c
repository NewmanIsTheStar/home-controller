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
FILE_METRICS_T picofs_metrics[FS_NUM_FID]; 


/*!
 * \brief Print a list of all files in the file system
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 if latest, 1 if not latest sequence
 */
int picofs_is_latest_file_sequence(char *filename, u8_t file_id, u8_t sequence)
{
    int islatest = 1;
    int i;
    u8_t *p = NULL;
    FILE_TRAILER_T *t = NULL;

    //picofs_printf("Checking %s seq %d\n", filename, sequence);


    // scan backwards through flash
    p = FS_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash
    while (((char *)p) >= FS_START)
    {
        t = (FILE_TRAILER_T *)p;

        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION) &&
            /*(strcmp(t->name, filename) == 0)*/ t->file_id == file_id)
        {
            if (t->file_sequence > sequence)  // TODO: handle wrap around
            {
               islatest = 0;  // we found a later sequence
               break;
            }

            p = p - t->file_size;  
        }
        else
        {
            p--;
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

    // if file_id is valid and cache is allocated
    if ((file_id != FS_INVALID_FID) && custom_fds[fd].cache)
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

        // store trailer in the cached file too (this is used to bootstrap the second initialization of the fd from cache)
        memcpy(custom_fds[fd].cache, (char *)&(custom_fds[fd].cache_trailer), sizeof(FILE_TRAILER_T));

        err = 0;
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
    int err = -1;
    u8_t *p = NULL;
    FILE_TRAILER_T *t = NULL;
    u8_t file_id_map[32];
    u8_t file_id_bit;
    u8_t file_id_byte;
    u8_t file_id = FS_INVALID_FID;
    FILE_TRAILER_T *trailer;
    bool found = false;

    // initialize file id map
    memset(file_id_map, 0, sizeof(file_id_map));

    // scan backwards through flash
    p = FS_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash and create bitmap of all used file_id numbers
    while (((char *)p) >= FS_START)
    {
        t = (FILE_TRAILER_T *)p;

        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION))
        {
            // update file_id bitmap
            file_id_byte = t->file_id/8;
            file_id_bit = t->file_id%8;
            file_id_map[file_id_byte] |= (1<<file_id_bit);

            p = p - t->file_size;  
        }
        else
        {
            p--;
        }
    }

    // scan file_id bitmap to find an unused file_id
    for (file_id=0; file_id < FS_NUM_FID; file_id++)
    {
        file_id_byte = file_id/8;
        file_id_bit = file_id%8;
        
        if (!(file_id_map[file_id_byte] & (1<<file_id_bit)))
        {
            break;
        }        
    }

    return(file_id);
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
int picofs_iter_next_file(FILE_TRAILER_T **current_file)    //TODO CHECK CRC! at present an erased hole in the file that is filled with new files will be skipped over!!!
{
    int not_found = 1;
    char *p = NULL;
    FILE_TRAILER_T *t = NULL;


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
            (t->crc == calculate_crc32_universal_unaligned_rtos(((const uint8_t *)(p + sizeof(FILE_TRAILER_T) - t->file_size)), t->file_size - sizeof(FILE_TRAILER_T))))
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
            (t->picofs_version == FS_VERION))
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
 * \brief Print a list of all files in the file system
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_refresh_metrics(void)
{
    int err = -1;
    FILE_TRAILER_T *current = NULL;

    memset(picofs_metrics, 0, sizeof(picofs_metrics));

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            picofs_metrics[current->file_id].valid = true;

            if (current->file_sequence >= picofs_metrics[current->file_id].trailer->file_sequence)    // TODO proper handling of sequence wrap around!
            {
                picofs_metrics[current->file_id].trailer = current;
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
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
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
        return(false);
    }

    while(!picofs_iter_next_file(&current))
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
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
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
