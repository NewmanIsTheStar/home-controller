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
u8_t picofs_find_file_at_location(char *search);
int picofs_iter_next_file(char *current_file);

// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];

//static variables
FILE_TEST_T test_filesystem[10];
 

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
 * \return 0 on success
 */
int picofs_list_all_files(void)
{
    int err = -1;
    int i;
    u8_t *p = NULL;
    FILE_TRAILER_T *t = NULL;

    // scan backwards through flash
    p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash
    while (((char *)p) >= FS_FLASH_START)
    {
        t = (FILE_TRAILER_T *)p;

        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION))
        {
            //picofs_printf("CHECKING %08d\t%d\t%s\n", h->file_size, h->file_sequence, h->name);
            if (picofs_is_latest_file_sequence(t->name, t->file_id, t->file_sequence))
            {
                if (!t->file_status)  // not deleted
                {
                    picofs_printf("%08d\t%d\t%d\t%s\n", t->file_size, t->file_id, t->file_sequence, t->name);
                }
            }

            p = p - t->file_size;  
        }
        else
        {
            p--;
        }
    }

    // // scan cache
    // for (i = 0; i < FS_MAX_FILE_DESCRIPTORS; i++) 
    // {
    //     if (custom_fds[i].cache) 
    //     {
    //         h = (FILE_HEADER_T *)custom_fds[i].cache;

    //         if ((strncmp(h->magic_number, "pfs", 4) == 0) &&
    //             (h->picofs_version == FS_VERION))
    //         {
    //             picofs_printf("%08d\t%d\t%s*\n", h->file_size, h->file_sequence, h->name);
    //         }
    //     }
    // }   
    
    {   // TEST TEST TEST 
        u8_t file_id_list[10];
        int file_size_list[10];

        picofs_list_files_within_size_range(32, 256,  file_id_list, file_size_list, NUM_ROWS(file_id_list));
    }

    return(err);
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
        memset((void *)test_filesystem, 255, sizeof(test_filesystem));

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
        for (i=0; i<216; i++)
        {
            STRNCAT(test_filesystem[0].test_data, "A", sizeof(test_filesystem[0].test_data));
        }
        test_filesystem[0].test_trailer.file_size = sizeof(FILE_TRAILER_T) + strlen(test_filesystem[0].test_data) + 1;
        // STRNCPY(test_filesystem[0].test_trailer.magic_number, "spf", sizeof(test_filesystem[0].test_trailer.magic_number));
        // test_filesystem[0].test_trailer.crc = 0;

        STRNCPY(test_filesystem[1].test_trailer.magic_number, "pfs", sizeof(test_filesystem[1].test_trailer.magic_number));
        test_filesystem[1].test_trailer.picofs_version = 0;
        test_filesystem[1].test_trailer.file_id = 1;    
        test_filesystem[1].test_trailer.file_sequence = 1;  
        test_filesystem[1].test_trailer.file_status = 0; 
        test_filesystem[1].test_trailer.file_size = 0; 
        test_filesystem[1].test_trailer.crc = 0;
        STRNCPY(test_filesystem[1].test_trailer.name, "monkey", sizeof("monkey"));
        //STRNCPY(test_filesystem[1].test_data, "This is test file B.", sizeof("This is test file B.")); 
        test_filesystem[1].test_data[0] = 0;
        for (i=0; i<216; i++)
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

    start_tick = xTaskGetTickCount();

    for(cell = FLASH_SCAN_START; cell < FLASH_SCAN_END;)
    {

        erase_block_absolute = ((u32_t)cell)/4096;
        erase_block_relative = (u32_t)(cell - FLASH_SCAN_START)/4096;            
        page_relative = ((u32_t)(cell - FLASH_SCAN_START)%4096)/256;  
        

        switch(display)
        {
        case PFS_DISPLAY_PAGE_MAP:
            if (((((u32_t)cell)%4096) == 0))
            {
                printf("\n[Block%04d]", erase_block_relative);
            }        
            break;
        case PFS_DISPLAY_SHELL_PAGE_MAP:
            if (((((u32_t)cell)%4096) == 0))
            {
                shell_printf("\n[Block%04d]", erase_block_relative);
            }  
            break;
        default:
            break;
        }

        if (*cell != 0xFF)
        {
            switch(display)
            {
            default:
            case PFS_DISPLAY_QUIET:
                break;
            case PFS_DISPLAY_PAGE_NUMBERS:
                // printf("[Block%04d, Page%02d]\tused\n", erase_block_relative, page_relative);
                printf("[Block%04d, Page%02d]\tused by %d\n", erase_block_relative, page_relative, picofs_find_file_at_location(cell));
                break;
            case PFS_DISPLAY_PAGE_MAP:
                printf("\u25A0");
                break;
            case PFS_DISPLAY_SHELL_PAGE_NUMBERS:
                //shell_printf("[Block%04d, Page%02d]\tused\n", erase_block_relative, page_relative);
                shell_printf("[Block%04d, Page%02d]\tused by %d\n", erase_block_relative, page_relative, picofs_find_file_at_location(cell));
                //hex_dump((char *)test_filesystem, 512);
                break;            
            case PFS_DISPLAY_SHELL_PAGE_MAP:
                shell_printf("\u25A0");
                break;
            }
            
            // skip to next page
            cell = FLASH_SCAN_START + erase_block_relative*4096+((page_relative+1)*256);
        }
        else if (((cell - FLASH_SCAN_START)%256) == 255)
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
    total_pages = (FLASH_SCAN_END - FLASH_SCAN_START)/256;

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

    contiguous_pages_required = size/256 + size%256?1:0;

    for(cell = FLASH_SCAN_START; cell < FLASH_SCAN_END;)
    {
        erase_block_absolute = ((u32_t)cell)/4096;
        erase_block_relative = (u32_t)(cell - FLASH_SCAN_START)/4096;            
        page_relative = ((u32_t)(cell - FLASH_SCAN_START)%4096)/256;          
        
        if (*cell != 0xFF)
        {            
            // skip to next page
            cell = FLASH_SCAN_START + erase_block_relative*4096+((page_relative+1)*256);

            // reset counter
            contiguous_pages_found = 0;

            // remember start address
            *start_of_area = cell;
        }
        else if (((cell - FLASH_SCAN_START)%256) == 255)
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
    u8_t file_id = 255;

    file_id = picofs_get_new_file_id();

    // if file_id is valid and cache is allocated
    if ((file_id != 255) && custom_fds[fd].cache)
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
 * \return file_id or 255 on failure
 */
u8_t picofs_get_new_file_id(void)
{
    int err = -1;
    u8_t *p = NULL;
    FILE_TRAILER_T *t = NULL;
    u8_t file_id_map[32];
    u8_t file_id_bit;
    u8_t file_id_byte;
    u8_t file_id = 255;
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
    for (file_id=0; file_id < 255; file_id++)
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
    memset(file_id_list, 255, sizeof(u8_t)*list_len);
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
 * \return file_id or 255 on failure
 */
u8_t picofs_find_file_at_location(char *search)
{
    int err = -1;
    char *p = NULL;
    FILE_TRAILER_T *t = NULL;
    u8_t file_id = 255;
    u8_t file_sequence = 0;    
    FILE_TRAILER_T *trailer;
    bool found = false;

    if ((search < FS_FLASH_START) || (search >= FS_FLASH_END))
    {
        // search location is invalid
        return (255);
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
            printf("Search began @ %p  found first file trailer @ %p which is a delta of %p\n", search, p, p-search);
            // check that file trailer is for a file that overlaps our search location 
            if ((char *)(p /*+ sizeof(FILE_TRAILER_T)*/ - t->file_size) <= search)
            {
                file_id = t->file_id;
                file_sequence = t->file_sequence;                
            }
            else
            {
                printf("ignoring file %s because its size %0x does not overlap the search start point\n", t->name, t->file_size);
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
int picofs_iter_next_file(char *current_file)
{
    int not_found = -1;
    char *p = NULL;
    FILE_TRAILER_T *t = NULL;


    if ((current_file < FS_FLASH_START) || (current_file >= FS_FLASH_END))
    {
        p = FS_FLASH_END - 1 - sizeof(FILE_TRAILER_T);
    }
    else
    {
        p = current_file;
    }

    // scan backwards through flash until we find next file
    do
    {
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
        if ((strncmp(t->magic_number, "pfs", 4) == 0) &&
            (t->picofs_version == FS_VERION))
        {
            not_found = 0;
            break;
        }

    } while ((p >= FS_FLASH_START) && not_found);
    
    if (!not_found)
    {
        current_file = p;
    }

    return(not_found);
}