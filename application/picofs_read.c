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
u8_t picofs_list_files_within_size_range(int size_lo, int size_hi, u8_t *file_id_list, int *file_size_list, int list_len);
u8_t picofs_find_file_at_location(char *search, FILE_TRAILER_T **trailer);
int picofs_iter_next_file(FILE_TRAILER_T **current_file);
int picofs_refresh_metrics(void);
int picofs_ascending_size_compare(const void *a, const void *b);
int picofs_descending_size_compare(const void *a, const void *b);
u32_t picofs_get_start_block(FILE_TRAILER_T *trailer);
u32_t picofs_get_end_block(FILE_TRAILER_T *trailer);

// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];

//static variables
FILE_TEST_T test_filesystem[40];
FILE_METRICS_T picofs_metrics[FS_NUM_FID]; 

int picofs_read(int fd, char *ptr, int len)
{
    int i;

    for(i=0; i<len; i++)
    {       
        if ((custom_fds[fd].data_offset + i) < custom_fds[fd].data_len)
        {
            ptr[i] = custom_fds[fd].data[custom_fds[fd].data_offset + i];
        }
        else
        {
            break;
        }
    }

    return(i);
}

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
    p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash
    while (((char *)p) >= FS_FLASH_START)
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


int picofs_load_test_data(void)
{
    static bool test_init = false;
    int i;

    if (!test_init)
    {
        memset((void *)test_filesystem, FS_ERASED_CELL_VALUE, sizeof(test_filesystem));

        STRNCPY(test_filesystem[0].test_trailer.magic_number, "pfs", sizeof(test_filesystem[0].test_trailer.magic_number));
        test_filesystem[0].test_trailer.picofs_version = 0;
        test_filesystem[0].test_trailer.file_id = 0;    
        test_filesystem[0].test_trailer.file_sequence = 0;  
        test_filesystem[0].test_trailer.file_status = 0; 
        test_filesystem[0].test_trailer.file_size = 0; 
        test_filesystem[0].test_trailer.crc = 0;
        STRNCPY(test_filesystem[0].test_trailer.name, "elephant", sizeof("elephant"));  
        // STRNCPY(test_filesystem[0].test_data, "This is test file A.", sizeof("This is test file A.")); 
        
        test_filesystem[0].test_data[0] = 0;
        for (i=0; i<224; i++)
        {
            STRNCAT(test_filesystem[0].test_data, "A", sizeof(test_filesystem[0].test_data));
        }
        test_filesystem[0].test_trailer.file_size = sizeof(FILE_TRAILER_T) + strlen(test_filesystem[0].test_data) + 1;
        // STRNCPY(test_filesystem[0].test_trailer.magic_number, "spf", sizeof(test_filesystem[0].test_trailer.magic_number));
        // test_filesystem[0].test_trailer.crc = 0;

        STRNCPY(test_filesystem[1].test_trailer.magic_number, "pfs", sizeof(test_filesystem[1].test_trailer.magic_number));
        test_filesystem[1].test_trailer.picofs_version = 0;
        test_filesystem[1].test_trailer.file_id = 1;    
        test_filesystem[1].test_trailer.file_sequence = 253;  
        test_filesystem[1].test_trailer.file_status = 0; 
        test_filesystem[1].test_trailer.file_size = 0; 
        test_filesystem[1].test_trailer.crc = 0;
        STRNCPY(test_filesystem[1].test_trailer.name, "monkey", sizeof("monkey"));
        //STRNCPY(test_filesystem[1].test_data, "This is test file B.", sizeof("This is test file B.")); 
        test_filesystem[1].test_data[0] = 0;
        for (i=0; i<224; i++)
        {
            STRNCAT(test_filesystem[1].test_data, "B", sizeof(test_filesystem[1].test_data));
        }        
        test_filesystem[1].test_trailer.file_size = sizeof(FILE_TRAILER_T) + strlen(test_filesystem[1].test_data) + 1;
        // STRNCPY(test_filesystem[1].test_trailer.magic_number, "spf", sizeof(test_filesystem[1].test_trailer.magic_number));    
        // test_filesystem[1].test_trailer.crc = 0; 
    
        test_init = true;
    }

    return (0);
}



// REAL FLASH
// #define FLASH_SCAN_START (0x10000000UL)
// #define FLASH_SCAN_END (0x10000000UL + PICO_FLASH_SIZE_BYTES)

// FAKE FLASH
#define FLASH_SCAN_START FS_FLASH_START
#define FLASH_SCAN_END FS_FLASH_END
/*!
 * \brief Identify the status of each flash page
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_find_page_status(PFS_DISPLAY_TYPE_T display)
{
    char *cell = NULL;
    u32_t erase_block_absolute = 0;
    u32_t erase_block_relative = 0;    
    u32_t page_relative = 0;
    u32_t free_pages = 0;
    u32_t total_pages = 0;
    TickType_t start_tick;
    TickType_t elapsed_ticks = 0;
    FILE_TRAILER_T *trailer = NULL;
    u8_t file_id = FS_INVALID_FID;
    u8_t file_sequence = 0;

    start_tick = xTaskGetTickCount();

    for(cell = FLASH_SCAN_START; cell < FLASH_SCAN_END;)
    {

        erase_block_absolute = ((u32_t)cell)/FS_ERASE_BLOCK_SIZE;
        erase_block_relative = (u32_t)(cell - FLASH_SCAN_START)/FS_ERASE_BLOCK_SIZE;            
        page_relative = ((u32_t)(cell - FLASH_SCAN_START)%FS_ERASE_BLOCK_SIZE)/FS_PAGE_SIZE;  
        

        switch(display)
        {
        case PFS_DISPLAY_PAGE_MAP:
            if (((((u32_t)(cell - FLASH_SCAN_START))%FS_ERASE_BLOCK_SIZE) == 0))
            {
                printf("\n[Block%04d]", erase_block_relative);
            }        
            break;
        case PFS_DISPLAY_SHELL_PAGE_MAP:
            if (((((u32_t)(cell - FLASH_SCAN_START))%FS_ERASE_BLOCK_SIZE) == 0))
            {
                shell_printf("\n[Block%04d]", erase_block_relative);
            }  
            break;
        default:
            break;
        }

        if (*cell != FS_ERASED_CELL_VALUE)
        {
            switch(display)
            {
            default:
            case PFS_DISPLAY_QUIET:
                break;
            case PFS_DISPLAY_PAGE_NUMBERS:
                // printf("[Block%04d, Page%02d]\tused\n", erase_block_relative, page_relative);
                file_id = picofs_find_file_at_location(cell, &trailer);
                if (trailer)
                {
                    file_sequence = trailer->file_sequence;
                }
                else
                {
                    file_sequence = 0;
                }
                printf("[Block%04d, Page%02d]\tused   FID%d SEQ%d\n", erase_block_relative, page_relative, file_id, file_sequence);
                break;
            case PFS_DISPLAY_PAGE_MAP:
                printf("\u25A0");
                break;
            case PFS_DISPLAY_SHELL_PAGE_NUMBERS:
                //shell_printf("[Block%04d, Page%02d]\tused\n", erase_block_relative, page_relative);
                file_id = picofs_find_file_at_location(cell, &trailer);
                if (trailer)
                {
                    file_sequence = trailer->file_sequence;
                }
                else
                {
                    file_sequence = 0;
                }
                shell_printf("[Block%04d, Page%02d]\tused   FID%d SEQ%d\n", erase_block_relative, page_relative, file_id, file_sequence);
                //hex_dump((char *)test_filesystem, 512);
                break;            
            case PFS_DISPLAY_SHELL_PAGE_MAP:
                shell_printf("\u25A0");
                break;
            }
            
            // skip to next page
            cell = FLASH_SCAN_START + erase_block_relative*FS_ERASE_BLOCK_SIZE+((page_relative+1)*FS_PAGE_SIZE);
        }
        else if (((cell - FLASH_SCAN_START)%FS_PAGE_SIZE) == (FS_PAGE_SIZE-1))  // reached last cell of page
        {
            switch(display)
            {
            default:
            case PFS_DISPLAY_QUIET:
                break;
            case PFS_DISPLAY_PAGE_NUMBERS:
                printf("[Block%04d, Page%02d]\tfree\n", erase_block_relative, page_relative);
                break;
            case PFS_DISPLAY_PAGE_MAP:
                printf("\u25A1");
                break;
            case PFS_DISPLAY_SHELL_PAGE_NUMBERS:
                shell_printf("[Block%04d, Page%02d]\tfree\n", erase_block_relative, page_relative);
                break;            
            case PFS_DISPLAY_SHELL_PAGE_MAP:
                shell_printf("\u25A1");
                break;                            
            }            
                        
            free_pages++;
            cell++;            
        }
        else
        {
            cell++;
        }
    }

    switch(display)
    {
    case PFS_DISPLAY_PAGE_MAP:
        printf("\n");      
        break;
    case PFS_DISPLAY_SHELL_PAGE_MAP:
        shell_printf("\n");
        break;
    default:
        break;
    }    

    elapsed_ticks = xTaskGetTickCount() - start_tick;
    total_pages = (FLASH_SCAN_END - FLASH_SCAN_START)/FS_PAGE_SIZE;

    switch(display)
    {
    default:
    case PFS_DISPLAY_QUIET:
        break;
    case PFS_DISPLAY_PAGE_NUMBERS:
    case PFS_DISPLAY_PAGE_MAP:
        printf("Free Pages = %d out of %d total pages (%d%%)\n", free_pages, total_pages, (free_pages*100)/total_pages);
        printf("flash scan completed in %d ms\n", elapsed_ticks);
        break;
    case PFS_DISPLAY_SHELL_PAGE_NUMBERS:          
    case PFS_DISPLAY_SHELL_PAGE_MAP:
        shell_printf("Free Pages = %d out of %d total pages (%d%%)\n", free_pages, total_pages, (free_pages*100)/total_pages);
        shell_printf("flash scan completed in %d ms\n", elapsed_ticks);
        break;                          
    }   
    

    // TEST TEST TEST 
    // picofs_erase_obsolete_blocks();

    return(0);
}

/*!
 * \brief Identify contiguous erased area large enough to hold size bytes
 * 
 * \param[in]   size             number of bytes
 * 
 * \param[out]  start_of_area    pointer to found area
 *  *     
 * \return 0 on success
 */
int picofs_find_contiguous_free_area(size_t size, u8_t **start_of_area)
{
    int err = 1;
    char *cell = NULL;
    u32_t erase_block_absolute = 0;
    u32_t erase_block_relative = 0;    
    u32_t page_relative = 0;
    u32_t free_pages = 0;
    u32_t total_pages = 0;
    TickType_t start_tick;
    TickType_t elapsed_ticks = 0;
    u32_t contiguous_pages_required = 0;
    u32_t contiguous_pages_found = 0;    

    start_tick = xTaskGetTickCount();

    contiguous_pages_required = size/FS_PAGE_SIZE + (size%FS_PAGE_SIZE?1:0);
    printf("seeking %d contiguous pages\n", contiguous_pages_required);

    for(cell = *start_of_area = FLASH_SCAN_START; cell < FLASH_SCAN_END;)
    {
        erase_block_absolute = ((u32_t)cell)/FS_ERASE_BLOCK_SIZE;
        erase_block_relative = (u32_t)(cell - FLASH_SCAN_START)/FS_ERASE_BLOCK_SIZE;            
        page_relative = ((u32_t)(cell - FLASH_SCAN_START)%FS_ERASE_BLOCK_SIZE)/FS_PAGE_SIZE;          
        
        if (*cell != FS_ERASED_CELL_VALUE)
        {            
            // skip to next page
            cell = FLASH_SCAN_START + erase_block_relative*FS_ERASE_BLOCK_SIZE+((page_relative+1)*FS_PAGE_SIZE);

            // reset counter
            contiguous_pages_found = 0;

            // remember start address
            *start_of_area = cell;
        }
        else if (((cell - FLASH_SCAN_START)%FS_PAGE_SIZE) == (FS_PAGE_SIZE-1))
        {                                  
            contiguous_pages_found++;
            free_pages++;
            cell++;            

            if (contiguous_pages_found == contiguous_pages_required)
            {
                err = 0;
                break;
            }
        }
        else
        {
            cell++;
        }
    }

    elapsed_ticks = xTaskGetTickCount() - start_tick;
    
    if (!err)
    {
        // printf("Found erased region starting @%p with size %d in %d ms\n", *start_of_area, size, elapsed_ticks);
        // hex_dump(*start_of_area, 16);
    }
    else
    {
        // printf("failed to find erased pages\n");
    }
    
    return(err);
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
    p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash and create bitmap of all used file_id numbers
    while (((char *)p) >= FS_FLASH_START)
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
 * \brief get a list of file ids within a specified size range
 * \param[in]   size_lo          minimum file size
 * \param[in]   size_hi          maximum file size
 * \param[out]   file_id_list    file id list
 * \param[out]   file_size_list  file size list
 * \param[out]   list_len        list length
 * \return number of files found list of file_id and sizes
 */
u8_t picofs_list_files_within_size_range(int size_lo, int size_hi, u8_t *file_id_list, int *file_size_list, int list_len)
{
    int err = -1;
    int i = 0;
    int num_matching_files = 0;
    u8_t *p = NULL;
    FILE_TRAILER_T *t = NULL;
    bool found = false;
    bool already_in_list = false;
    

    // initialize lists
    memset(file_id_list, FS_INVALID_FID, sizeof(u8_t)*list_len);
    memset(file_size_list, 0, sizeof(int)*list_len);

    // scan backwards through flash
    p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash
    while (((char *)p) >= FS_FLASH_START)
    {
        t = (FILE_TRAILER_T *)p;

        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION))
        {
            // check range
            if ((t->file_size >= size_lo) && (t->file_size < size_hi) && picofs_is_latest_file_sequence(t->name, t->file_id, t->file_sequence))
            {
                already_in_list = false;
                for(i=0; i<num_matching_files; i++)
                {
                    if (t->file_id == file_id_list[i])
                    {
                        already_in_list = true;
                        break;
                    }
                }

                if (!already_in_list)
                {
                    if (num_matching_files<list_len)
                    {
                        // add file to list 
                        file_id_list[num_matching_files] = t->file_id;
                        file_size_list[num_matching_files] = t->file_size;

                        printf("file_id = %d file_size = %d file_name = %s\n", file_id_list[num_matching_files], file_size_list[num_matching_files], t->name);
                        num_matching_files++;
                    }
                }
            }

            p = p - t->file_size;  
        }
        else
        {
            p--;
        }

        if (num_matching_files >= list_len)
        {
            // list full so abort search
            break;
        }
    }

    return(num_matching_files);
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

    if ((search < FS_FLASH_START) || (search >= FS_FLASH_END))
    {
        // search location is invalid
        return (FS_INVALID_FID);
    }

    // scan forwards through flash
    p = search;

    // scan flash
    while (p < FS_FLASH_END)
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
 * \brief iterator function to move to the next file in flash
 * \param[in]   current_file      pointer to current file or NULL to initiate new walk
 * 
 * \return 0 on success
 */
int picofs_iter_next_file(FILE_TRAILER_T **current_file)
{
    int not_found = 1;
    char *p = NULL;
    FILE_TRAILER_T *t = NULL;


    if (((char *)*current_file < FS_FLASH_START) || ((char *)*current_file >= FS_FLASH_END))
    {
        p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);
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
            (t->picofs_version == FS_VERION))
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

    } while ((p >= FS_FLASH_START) && not_found);
    
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
 * \brief iterator function to move to the next file in flash
 * \param[in]   current_file      pointer to current file or NULL to initiate new walk
 * 
 * \return 0 on success
 */
int picofs_ascending_size_compare(const void *a, const void *b)
{
    int size_a = 0;
    int size_b = 0;

    if (((FILE_METRICS_T *)a)->valid)
    {
        size_a = ((FILE_METRICS_T*)a)->trailer->file_size;
    }

    if (((FILE_METRICS_T *)b)->valid)
    {
        size_b = ((FILE_METRICS_T*)b)->trailer->file_size;
    }

    return (size_a - size_b);
}

/*!
 * \brief iterator function to move to the next file in flash
 * \param[in]   current_file      pointer to current file or NULL to initiate new walk
 * 
 * \return 0 on success
 */
int picofs_descending_size_compare(const void *a, const void *b)
{
    int size_a = 0;
    int size_b = 0;

    if (((FILE_METRICS_T *)a)->valid)
    {
        size_a = ((FILE_METRICS_T*)a)->trailer->file_size;
    }

    if (((FILE_METRICS_T *)b)->valid)
    {
        size_b = ((FILE_METRICS_T*)b)->trailer->file_size;
    }

    return (size_b - size_a);
}

/*!
 * \brief list files by size
 * 
 * \return 0 on success
 */
int picofs_list_files_by_size(void)
{
    int err = -1;
    int i;
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

    qsort(picofs_metrics, NUM_ROWS(picofs_metrics), sizeof(FILE_METRICS_T), picofs_ascending_size_compare);

    picofs_printf("LIST sorted by size\n");

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (picofs_metrics[i].valid)
        {
            picofs_printf("%08d\t%d\t%d\t%s\n", picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_id, picofs_metrics[i].trailer->file_sequence, picofs_metrics[i].trailer->name);
        }
    }

    return(0);
}

/*!
 * \brief compare file names
 * \param[in]   a      pointer to data structure to compare
 * \param[in]   b      pointer to data structure to compare 
 * \return 0 on success
 */
int picofs_metrics_name_compare(const void *a, const void *b)
{
    char *name_a = "";
    char *name_b = "";

    if (((FILE_METRICS_T *)a)->valid)
    {
        name_a = ((FILE_METRICS_T*)a)->trailer->name;
    }

    if (((FILE_METRICS_T *)b)->valid)
    {
        name_b = ((FILE_METRICS_T*)b)->trailer->name;
    }

    return (strcmp(name_a, name_b));
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


/*!
 * \brief Print a list of all files in the file system
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_list_all_files(void)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *current = NULL;
    int num_files = 0;
    int size_files = 0;
    int size_files_plus_remnants = 0;  // remnants include deleted files and old versions of files that are no longer visible but are taking up space in flash
    u8_t *consolidation_area = NULL;

    memset(picofs_metrics, 0, sizeof(picofs_metrics));

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            picofs_metrics[current->file_id].valid = true;
            num_files++;
            size_files_plus_remnants += current->file_size;

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

    qsort(picofs_metrics, NUM_ROWS(picofs_metrics), sizeof(FILE_METRICS_T), picofs_metrics_name_compare);

    if (num_files)
    {
            picofs_printf("Size\t\tFID\tSEQ\tName\n");
    }

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (picofs_metrics[i].valid && !picofs_metrics[i].trailer->file_status)
        {
            picofs_printf("%08d\t%d\t%d\t%s\n", picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_id, picofs_metrics[i].trailer->file_sequence, picofs_metrics[i].trailer->name);
            size_files += picofs_metrics[i].trailer->file_size;
        }
    }

    picofs_printf("\nTotal size    %08d\n", size_files);
    picofs_printf("Remnants size %08d\n", size_files_plus_remnants - size_files);

    picofs_printf("Space to consolidate? %s\n", picofs_find_contiguous_free_area(size_files, &consolidation_area)?"NO":"YES");

    return(0);
}

/*!
 * \brief erase obsolete blocks
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_erase_obsolete_blocks(void)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *current = NULL;
    FILE_TRAILER_T *look_ahead = NULL;
    int num_files = 0;
    int size_files = 0;
    int size_files_plus_remnants = 0;  // remnants include deleted files and old versions of files that are no longer visible but are taking up space in flash
    u8_t *consolidation_area = NULL;
    u32_t file_start_block = 0;
    u32_t file_end_block = 0;
    bool erasure_possible = false;
    u32_t last_valid_start_block = UINT_MAX;

    picofs_refresh_metrics();

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            // printf("checking %s seq %d\n", current->name, current->file_sequence);

            if (picofs_metrics[current->file_id].trailer != current)
            {

                file_start_block = picofs_get_start_block(current);
                file_end_block = picofs_get_end_block(current);
                
                // printf("found remnant of file %s start block %d to end block %d\n", current->name, file_start_block, file_end_block);

                if (file_end_block != last_valid_start_block)
                {
                    erasure_possible = true;
                    look_ahead = current;

                    while(!picofs_iter_next_file(&look_ahead))
                    {
                        if (picofs_get_end_block(look_ahead) == file_start_block)
                        {
                            if (picofs_metrics[look_ahead->file_id].trailer != look_ahead)
                            {
                                // printf("lookahead found adjacent obsolete file in the same block\n");
                                // adjacent obsolete file so expand range to cover it  
                                file_start_block = picofs_get_start_block(look_ahead); 
                                current = look_ahead;
                            }
                            else
                            {
                                // printf("lookahead found adjacent valid file in the same block\n");
                                // adjacent valid file so we cannot erase blocks that contain the valid file
                                if (file_end_block > file_start_block)
                                {
                                    // eliminate block that contains both files from erasure range
                                    file_start_block++;

                                    // printf("file start block incremented\n");
                                }
                                else
                                {
                                    // no erasure possible because first block of obsolete file also contains a valid file
                                    erasure_possible = false;

                                    // printf("erasure not possible\n");
                                }
                                break;
                            }
                        }
                        else
                        {
                            // printf("end block and start block not the same -- break\n");
                            
                            break;
                        }
                    }

                    if (erasure_possible)
                    {
                        if (file_start_block == file_end_block)
                        {
                            shell_printf("ERASING Block %d\n", file_start_block);
                        }
                        else
                        {
                            shell_printf("ERASING Blocks %d to %d\n", file_start_block, file_end_block);
                        }
                        
                        picofs_erase_block_range(file_start_block, file_end_block);
                    }
                }
            }
            else
            {
                last_valid_start_block = picofs_get_start_block(current);
                // printf("found valid file with start block %d\n", last_valid_start_block);
            }
        }
    }

    return(0);
}

/*!
 * \brief check if erase block is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
u32_t picofs_get_start_block(FILE_TRAILER_T *trailer)
{
    u32_t start_block;
    
    start_block = ((((char *)trailer + sizeof(FILE_TRAILER_T)) - trailer->file_size) - FS_FLASH_START)/FS_ERASE_BLOCK_SIZE; 

    return(start_block);
}

/*!
 * \brief check if erase block is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
u32_t picofs_get_end_block(FILE_TRAILER_T *trailer)
{
    u32_t end_block;
    
    end_block = (((char *)trailer + sizeof(FILE_TRAILER_T) - 1) - FS_FLASH_START)/FS_ERASE_BLOCK_SIZE; 

    return(end_block);
}

/*!
 * \brief consolidate all files in the file system into a sequential block
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_consolidate_all_files(void)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *current = NULL;
    int num_files = 0;
    int size_files = 0;
    int size_files_plus_remnants = 0;  // remnants include deleted files and old versions of files that are no longer visible but are taking up space in flash
    u8_t *consolidation_area = NULL;
    int total_written = 0;

    memset(picofs_metrics, 0, sizeof(picofs_metrics));

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            picofs_metrics[current->file_id].valid = true;
            num_files++;
            size_files_plus_remnants += current->file_size;

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

    qsort(picofs_metrics, NUM_ROWS(picofs_metrics), sizeof(FILE_METRICS_T), picofs_ascending_size_compare);

    if (num_files)
    {
            picofs_printf("Size\t\tFID\tSEQ\tName\n");
    }

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (picofs_metrics[i].valid && !picofs_metrics[i].trailer->file_status)
        {
            picofs_printf("%08d\t%d\t%d\t%s\n", picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_id, picofs_metrics[i].trailer->file_sequence, picofs_metrics[i].trailer->name);
            size_files += picofs_metrics[i].trailer->file_size;
        }
    }

    picofs_printf("\nTotal size    %08d\n", size_files);
    picofs_printf("Remnants size %08d\n", size_files_plus_remnants - size_files);

    picofs_printf("Space to consolidate? %s\n", picofs_find_contiguous_free_area(size_files, &consolidation_area)?"NO":"YES");

    // picofs_printf("consolidation_area = %p\n", consolidation_area);

    if (!consolidation_area)
    {
        shell_printf("picofs: attempt to free up space by erasing obsolete blocks\n");
        picofs_erase_obsolete_blocks();
        picofs_printf("After erasure, space to consolidate? %s\n", picofs_find_contiguous_free_area(size_files, &consolidation_area)?"NO":"YES");
    }

    total_written = 0;

    if (consolidation_area)
    {
        for(i=0; i<FS_NUM_FID; i++)
        {
            if (picofs_metrics[i].valid && picofs_metrics[i].trailer)
            {
                printf("copying...\n");
                hex_dump((u8_t *)((char *)picofs_metrics[i].trailer + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size), picofs_metrics[i].trailer->file_size);
                memcpy(consolidation_area + total_written, (char *)picofs_metrics[i].trailer + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_size);
                total_written += picofs_metrics[i].trailer->file_size;
                shell_printf("picofs: consolidated file: %s %d bytes\n", picofs_metrics[i].trailer->name, picofs_metrics[i].trailer->file_size);
            }

            if(total_written >= size_files)
            {
                printf("picofs: consolidate: reached total consolidated file size of %d bytes @ FID %d\n", size_files, picofs_metrics[i].trailer->file_id);
                break;
            }
        }
    }

    shell_printf("picofs: consolidation completed consolidated size is %d bytes (out of %d bytes)\n", total_written, size_files);

    if (total_written == size_files)
    {
        err = 0;
    }

    return(err);
}

/*!
 * \brief erase a sequential set of blocks
 * 
 * \param[in]   start_block     first block to erase
 * 
 * \param[out]  end_block       last block to erase
 *  *     
 * \return 0 on success
 */
int picofs_erase_block_range(int start_block, int end_block)
{
    int err = 0;
    
    memset(FS_FLASH_START + start_block*FS_ERASE_BLOCK_SIZE, 255, (end_block-start_block+1)*FS_ERASE_BLOCK_SIZE);

    return(err);
}

/*!
 * \brief consolidate as many files as possible to the given buffer
 * 
 * \param[in]   buffer       buffer to place output
 * \param[out]  len          size of buffer
 * \param[out]  exclude_fid  FID to exclude (use 255 to not exlude anything)  
 * \return 0 on success
 */
int picofs_consolidate_files_to_buffer(char * buffer, int len, u8_t exclude_fid)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *current = NULL;
    int num_files = 0;
    int size_files = 0;
    int size_files_plus_remnants = 0;  // remnants include deleted files and old versions of files that are no longer visible but are taking up space in flash
    u8_t *consolidation_area = NULL;
    int total_written = 0;

    memset(picofs_metrics, 0, sizeof(picofs_metrics));
    memset(buffer, FS_ERASED_CELL_VALUE, len); 

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            picofs_metrics[current->file_id].valid = true;
            num_files++;
            size_files_plus_remnants += current->file_size;

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

    qsort(picofs_metrics, NUM_ROWS(picofs_metrics), sizeof(FILE_METRICS_T), picofs_descending_size_compare);

    if (num_files)
    {
            picofs_printf("Size\t\tFID\tSEQ\tName\n");
    }

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (picofs_metrics[i].valid && !picofs_metrics[i].trailer->file_status)
        {
            picofs_printf("%08d\t%d\t%d\t%s\n", picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_id, picofs_metrics[i].trailer->file_sequence, picofs_metrics[i].trailer->name);
            size_files += picofs_metrics[i].trailer->file_size;
        }
    }

    picofs_printf("\nTotal size    %08d\n", size_files);
    picofs_printf("Remnants size %08d\n", size_files_plus_remnants - size_files);

    total_written = 0;
    consolidation_area = buffer;

    if (consolidation_area)
    {
        for(i=0; i<FS_NUM_FID; i++)
        {
            if ((picofs_metrics[i].valid && picofs_metrics[i].trailer) &&
                (picofs_metrics[i].trailer->file_size < (len - total_written)) &&
                (picofs_metrics[i].trailer->file_id != exclude_fid) &&
                (picofs_metrics[i].trailer->file_sequence < (FS_MAX_SEQ-1)))
            {
                // normal consolidation
                printf("copying...\n");
                hex_dump((u8_t *)((char *)picofs_metrics[i].trailer + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size), picofs_metrics[i].trailer->file_size);
                memcpy(consolidation_area + total_written, (char *)picofs_metrics[i].trailer + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_size);
                picofs_increment_sequence((FILE_TRAILER_T *)(consolidation_area + total_written + picofs_metrics[i].trailer->file_size - sizeof(FILE_TRAILER_T)));
                total_written += picofs_metrics[i].trailer->file_size;
                shell_printf("picofs: consolidated file: %s %d bytes\n", picofs_metrics[i].trailer->name, picofs_metrics[i].trailer->file_size);
            }
            else if ((picofs_metrics[i].valid && picofs_metrics[i].trailer) &&
                     ((picofs_metrics[i].trailer->file_size + sizeof(FILE_TRAILER_T)) < (len - total_written)) &&
                     (picofs_metrics[i].trailer->file_id != exclude_fid) &&
                     (picofs_metrics[i].trailer->file_sequence == (FS_MAX_SEQ-1)))
            {
                // special case: consolidation with FID rollover during consolidation, requires extra space for a trailer marking file deletion 
                printf("copying...\n");
                hex_dump((u8_t *)((char *)picofs_metrics[i].trailer + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size), picofs_metrics[i].trailer->file_size);
                memcpy(consolidation_area + total_written, (char *)picofs_metrics[i].trailer + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_size);
                if (!picofs_increment_sequence((FILE_TRAILER_T *)(consolidation_area + total_written + picofs_metrics[i].trailer->file_size - sizeof(FILE_TRAILER_T))))
                {
                    total_written += picofs_metrics[i].trailer->file_size;
                    shell_printf("picofs: consolidated file with FID rollover: %s %d bytes\n", picofs_metrics[i].trailer->name, picofs_metrics[i].trailer->file_size);

                    // append empty file marking the deletion of the old FID that had run out of sequence numbers
                    memcpy(consolidation_area + total_written, (char *)picofs_metrics[i].trailer, sizeof(FILE_TRAILER_T));
                    ((FILE_TRAILER_T *)(consolidation_area + total_written))->file_status = 1;                     // mark for deletion
                    ((FILE_TRAILER_T *)(consolidation_area + total_written))->file_size = sizeof(FILE_TRAILER_T);  // empty file
                    ((FILE_TRAILER_T *)(consolidation_area + total_written))->file_sequence = FS_MAX_SEQ;         // last sequence
                    total_written += sizeof(FILE_TRAILER_T);
                }
                else
                {
                    printf("Skipped file for speculative consolidation because we ran out of FIDs to rollover a file with sequence 254\n");
                }
            }            
            else
            {
                if (picofs_metrics[i].valid)
                {
                    printf("Skipped file for speculative consolidation because: size %d vs %d | fid %d vs %d | ptr %p | seq %d\n", picofs_metrics[i].trailer->file_size, (len - total_written), picofs_metrics[i].trailer->file_id, exclude_fid, picofs_metrics[i].trailer,  picofs_metrics[i].trailer->file_sequence);
                }
            }

            if(total_written >= len)
            {
                printf("picofs: consolidate: reached total consolidated size of %d bytes @ FID %d\n", len, picofs_metrics[i].trailer->file_id);
                break;
            }
        }
    }

    shell_printf("picofs: consolidation completed total size is %d bytes\n", total_written);

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