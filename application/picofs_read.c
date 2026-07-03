/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>


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
    u8_t *p = FS_FLASH_START;
    FILE_HEADER_T *h = NULL;

    // scan flash
    while (((char *)p) < (FS_FLASH_END - sizeof(FILE_HEADER_T) - sizeof(FILE_TRAILER_T)))
    {        
        h = (FILE_HEADER_T *)p;

        if ((strncmp(h->magic_number, "pfs", 4) == 0) &&
            (h->picofs_version == FS_VERION))
        {
            //picofs_printf("CHECKING %08d\t%d\t%s\n", h->file_size, h->file_sequence, h->name);
            if (picofs_is_latest_file_sequence(h->name, h->file_sequence))
            {
                if (!h->file_status)  // not deleted
                {
                    picofs_printf("%08d\t%d\t%s\n", h->file_size, h->file_sequence, h->name);
                }
            }

            p += h->file_size;
        }
        else
        {
            p++;
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

    return(err);
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
int picofs_is_latest_file_sequence(char *filename, u8_t sequence)
{
    int found = 1;
    int i;
    u8_t *p = FS_FLASH_START;
    FILE_HEADER_T *h = NULL;

    //picofs_printf("Checking %s seq %d\n", filename, sequence);

    // scan flash
    while (((char *)p) < (FS_FLASH_END - sizeof(FILE_HEADER_T) - sizeof(FILE_TRAILER_T)))
    {        
        h = (FILE_HEADER_T *)p;

        if ((strncmp(h->magic_number, "pfs", 4) == 0) &&
            (h->picofs_version == FS_VERION) &&
            (strcmp(h->name, filename) == 0))
        {
            if (h->file_sequence > sequence)  // TODO: handle wrap around
            {
               found = 0;  // we found a later sequence
               break;
            }

            p += h->file_size;
        }
        else
        {
            p++;
        }
    }

    return(found);
}


int picofs_load_test_data(void)
{
    static bool test_init = false;

    if (!test_init)
    {
        memset((void *)test_filesystem, 255, sizeof(test_filesystem));

        STRNCPY(test_filesystem[0].test_header.magic_number, "pfs", sizeof(test_filesystem[0].test_header.magic_number));
        test_filesystem[0].test_header.picofs_version = 0;
        test_filesystem[0].test_header.file_id = 0;    
        test_filesystem[0].test_header.file_sequence = 0;  
        test_filesystem[0].test_header.file_status = 0; 
        test_filesystem[0].test_header.file_size = 0; 
        test_filesystem[0].test_header.crc = 0;
        STRNCPY(test_filesystem[0].test_header.name, "elephant", sizeof("elephant"));  
        STRNCPY(test_filesystem[0].test_data, "This is test file A.", sizeof("This is test file A.")); 
        test_filesystem[0].test_header.file_size = sizeof(FILE_HEADER_T) + strlen(test_filesystem[0].test_data) + 1 + sizeof(FILE_TRAILER_T);
        STRNCPY(test_filesystem[0].test_trailer.magic_number, "spf", sizeof(test_filesystem[0].test_trailer.magic_number));
        test_filesystem[0].test_trailer.crc = 0;

        STRNCPY(test_filesystem[1].test_header.magic_number, "pfs", sizeof(test_filesystem[1].test_header.magic_number));
        test_filesystem[1].test_header.picofs_version = 0;
        test_filesystem[1].test_header.file_id = 0;    
        test_filesystem[1].test_header.file_sequence = 1;  
        test_filesystem[1].test_header.file_status = 0; 
        test_filesystem[1].test_header.file_size = 0; 
        test_filesystem[1].test_header.crc = 0;
        STRNCPY(test_filesystem[1].test_header.name, "monkey", sizeof("monkey"));
        STRNCPY(test_filesystem[1].test_data, "This is test file B.", sizeof("This is test file B.")); 
        test_filesystem[1].test_header.file_size = sizeof(FILE_HEADER_T) + strlen(test_filesystem[1].test_data) + 1 + sizeof(FILE_TRAILER_T);
        STRNCPY(test_filesystem[1].test_trailer.magic_number, "spf", sizeof(test_filesystem[1].test_trailer.magic_number));    
        test_filesystem[1].test_trailer.crc = 0; 
    
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
    u8_t *cell = NULL;
    u32_t erase_block_absolute = 0;
    u32_t erase_block_relative = 0;    
    u32_t page_relative = 0;
    u32_t free_pages = 0;
    u32_t total_pages = 0;
    TickType_t start_tick;
    TickType_t elapsed_ticks = 0;

    start_tick = xTaskGetTickCount();

    for(cell = (u8_t *)(FLASH_SCAN_START); cell < (u8_t *)(FLASH_SCAN_END);)
    {

        erase_block_absolute = ((u32_t)cell)/4096;
        erase_block_relative = ((u32_t)cell - (u32_t)FLASH_SCAN_START)/4096;            
        page_relative = (((u32_t)cell)%4096)/256;
        

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
                printf("[Block%04d, Page%02d]\tused\n", erase_block_relative, page_relative);
                break;
            case PFS_DISPLAY_PAGE_MAP:
                printf("\u25A0");
                break;
            case PFS_DISPLAY_SHELL_PAGE_NUMBERS:
                shell_printf("[Block%04d, Page%02d]\tused\n", erase_block_relative, page_relative);
                break;            
            case PFS_DISPLAY_SHELL_PAGE_MAP:
                shell_printf("\u25A0");
                break;
            }
            
            // skip next page
            cell = (u8_t *)(erase_block_absolute*4096+((page_relative+1)*256));
        }
        else if (((u32_t)cell%256) == 255)
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
    u8_t *cell = NULL;
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

    // size += sizeof(FILE_HEADER_T);
    // size += sizeof(FILE_TRAILER_T);    
    
    contiguous_pages_required = size/256 + size%256?1:0;

    for(cell = (u8_t *)(FLASH_SCAN_START); cell < (u8_t *)(FLASH_SCAN_END);)
    {

        erase_block_absolute = ((u32_t)cell)/4096;
        erase_block_relative = ((u32_t)cell - (u32_t)FLASH_SCAN_START)/4096;            
        page_relative = (((u32_t)cell)%4096)/256;
        
        if (*cell != 0xFF)
        {            
            // skip next page
            cell = (u8_t *)(erase_block_absolute*4096+((page_relative+1)*256));

            // reset counter
            contiguous_pages_found = 0;

            // remember start address
            *start_of_area = cell;
        }
        else if (((u32_t)cell%256) == 255)
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
 * \brief Print a list of all files in the file system
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_create_file_header(int fd, char *name)
{
    int err = -1;
    u8_t *p = FS_FLASH_START;
    //  u8_t *next = NULL; 
    FILE_HEADER_T *h = NULL;
    u8_t best_sequence = 0;
    bool first_sequnce = false;
    u8_t file_id_map[32];
    u8_t file_id_bit;
    u8_t file_id_byte;
    u8_t file_id = 255;
    FILE_HEADER_T *header;

    // initialize file id map
    memset(file_id_map, 0, sizeof(file_id_map));

    // scan flash to find all file_ids and record them in a bitmap
    while (((char *)p) < (FS_FLASH_END - sizeof(FILE_HEADER_T) - sizeof(FILE_TRAILER_T)))
    {        
        h = (FILE_HEADER_T *)p;

        if ((strncmp(h->magic_number, "pfs", 4) == 0) &&
            (h->picofs_version == FS_VERION))
        {
            // update file_id bitmap
            file_id_byte = h->file_id/8;
            file_id_bit = h->file_id%8;
            file_id_map[file_id_byte] |= (1<<file_id_bit);

            // check for deleted file with the same name!
            if ((strcmp(name, h->name) == 0) && (h->file_sequence > best_sequence))
            {
                best_sequence = h->file_sequence;
            }

            p += h->file_size;
        }
        else
        {
            p++;
        }
    }

    // we found a deleted file with the same name so start with the next sequence number when creating the new file
    if (best_sequence)
    {
        best_sequence++;
    }

    // TODO need to also scan all inuse file descriptor for file_ids existing only in cache <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

    // scan file_id bitmap to find an unused file_id
    for (file_id_byte=0; file_id_byte < 32; file_id_byte++)
    {
        if (!(file_id_map[file_id_byte] & (1<<file_id_bit)))
        {
            file_id = file_id_byte*8 + file_id_bit;
            break;
        }
    }

    // if file_id is valid and cache is allocated
    if ((file_id != 255) && custom_fds[fd].cache)
    {
        header = (FILE_HEADER_T *)custom_fds[fd].cache;
        
        //TODO Consider MUTEX to prevent collision if two tasks try to create a file at once perhaps audit during erase when cpu locked
        //NB this is mainly caused by having a unique file_id that is not actually used at present
       
        STRNCPY(header->magic_number, "pfs", sizeof(header->magic_number));                                                            
        header->picofs_version = FS_VERION;
        header->file_id = file_id;
        header->file_sequence = best_sequence;
        header->file_status= 0;
        header->file_size = sizeof(FILE_HEADER_T) + sizeof(FILE_TRAILER_T);
        header->crc = 0;
        STRNCPY(header->name, name, sizeof(header->name));

        err = 0;
    }

    return(err);
}