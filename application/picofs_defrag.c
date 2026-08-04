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
//extern FILE_STATUS_T picofs_files[FS_NUM_FID]; 

//static variables
FILE_STATUS_T consoldation_files[FS_NUM_FID]; 


/*!
 * \brief consolidate all files in the file system into a sequential block
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_consolidate_all_files_in_flash(void)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *current = NULL;
    int num_files = 0;
    int size_files = 0;
    int size_files_plus_remnants = 0;  // remnants include deleted files and old versions of files that are no longer visible but are taking up space in flash
    u8_t *consolidation_area = NULL;
    size_t consolidation_area_size = 0;
    int total_written = 0;
    FILE_TRAILER_T modified_trailer;

    memset(consoldation_files, 0, sizeof(consoldation_files));

    while(!picofs_iter_next_file(&current, false))
    {
        if (current)
        {
            consoldation_files[current->file_id].valid = true;
            num_files++;
            size_files_plus_remnants += current->file_size;

            if (current->file_sequence >= consoldation_files[current->file_id].trailer->file_sequence)    // TODO proper handling of sequence wrap around!
            {
                consoldation_files[current->file_id].trailer = current;
            }
        }
        else
        {
            printf("picofs: error: next iter unexpectedly returned a NULL pointer without an error return value\n");
            break;
        }
    }

    qsort(consoldation_files, NUM_ROWS(consoldation_files), sizeof(FILE_STATUS_T), picofs_descending_size_compare);

    if (num_files)
    {
            picofs_printf("Size\t\tFID\tSEQ\tName\n");
    }

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (consoldation_files[i].valid && !consoldation_files[i].trailer->file_status)
        {
            picofs_printf("%08d\t%d\t%d\t%s\n", consoldation_files[i].trailer->file_size, consoldation_files[i].trailer->file_id, consoldation_files[i].trailer->file_sequence, consoldation_files[i].trailer->name);
            size_files += consoldation_files[i].trailer->file_size;
        }
    }

    picofs_printf("\nTotal size    %08d\n", size_files);
    picofs_printf("Remnants size %08d\n", size_files_plus_remnants - size_files);

    picofs_printf("Space to consolidate? %s\n", picofs_find_contiguous_free_area(size_files, &consolidation_area, &consolidation_area_size)?"NO":"YES");

    // picofs_printf("consolidation_area = %p\n", consolidation_area);

    if (!consolidation_area)
    {
        shell_printf("picofs: attempt to free up space by erasing obsolete blocks\n");
        picofs_erase_obsolete_sectors();

        if (picofs_find_contiguous_free_area(size_files, &consolidation_area, &consolidation_area_size))
        {
            picofs_printf("After erasure, we still have insufficent space to consolidate.  --ABORT--\n"); 

            //TODO: try asking for less space and try packing files in reverse order (smallest first) or dropping large files
        }
    }

    total_written = 0;

    if (consolidation_area)
    {
        for(i=0; i<FS_NUM_FID; i++)
        {
            if (consoldation_files[i].valid && consoldation_files[i].trailer)
            {
                printf("copying...\n");
                //hex_dump((u8_t *)((char *)consoldation_files[i].trailer + sizeof(FILE_TRAILER_T) - consoldation_files[i].trailer->file_size), consoldation_files[i].trailer->file_size);
                //memcpy(consolidation_area + total_written, (char *)picofs_metrics[i].trailer + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_size);
                
                // append file data and exclude trailer
                picofs_append_to_flash(consolidation_area, size_files, (char *)consoldation_files[i].trailer + sizeof(FILE_TRAILER_T) - consoldation_files[i].trailer->file_size, consoldation_files[i].trailer->file_size - sizeof(FILE_TRAILER_T));
                
                // make local copy of trailer and increment sequence
                memcpy(&modified_trailer, consoldation_files[i].trailer, sizeof(FILE_TRAILER_T));
                picofs_increment_sequence(&modified_trailer);

                // append modified trailer
                picofs_append_to_flash(consolidation_area, size_files, (char *)&modified_trailer, sizeof(FILE_TRAILER_T));
                
                total_written += consoldation_files[i].trailer->file_size;
                shell_printf("picofs: consolidated file: %s %d bytes\n", consoldation_files[i].trailer->name, consoldation_files[i].trailer->file_size);

                // check for FID rollover
                if (modified_trailer.file_id != consoldation_files[i].trailer->file_id)
                {
                    // roll over occured so need to delete old FID if space permits
                    if (consolidation_area_size > (size_files + sizeof(FILE_TRAILER_T)))
                    {
                        size_files += sizeof(FILE_TRAILER_T);

                        // create empty file indicating the deletion of old fid
                        modified_trailer.file_id = consoldation_files[i].trailer->file_id;
                        modified_trailer.file_sequence = FS_MAX_SEQ;
                        modified_trailer.file_status = 1;
                        modified_trailer.file_size = sizeof(FILE_TRAILER_T);
                        modified_trailer.crc = 0; //  CRC of an empty buffer is 0  NB crc covers data but not the trailer  

                        picofs_append_to_flash(consolidation_area, size_files, (char *)&modified_trailer, sizeof(FILE_TRAILER_T));
    
                        total_written += sizeof(FILE_TRAILER_T);    
                        
                        printf("Deleted inline name = %s FID = %d\n", modified_trailer.name, modified_trailer.file_sequence);
                    }
                    else
                    {
                        consoldation_files[i].pending_deletion = true;
                    }
                }
            }

            if(total_written >= size_files)
            {
                printf("picofs: consolidate: reached total consolidated size of %d bytes @ FID %d\n", size_files, consoldation_files[i].trailer->file_id);
                break;
            }
        }

        // flush the last page to flash
        picofs_append_to_flash(NULL, 0, NULL, 0);

        // find and delete all duplicates created by rollovers that could not be handled during consolidation
        for (i=0; i<FS_NUM_FID; i++)
        {        
            if ((consoldation_files[i].valid) && (consoldation_files[i].trailer) && (consoldation_files[i].pending_deletion))
            {
                picofs_unlink(consoldation_files[i].trailer->name, consoldation_files[i].trailer->file_id);
                consoldation_files[i].pending_deletion = false;
                shell_printf("picofs: cleanup after FID rollover of %s\n", consoldation_files[i].trailer->name); 
            }
        }
    }

    shell_printf("picofs: consolidation completed consolidated size is %d bytes (out of %d bytes)\n", total_written, size_files);

    if (total_written == size_files)
    {
        err = 0;
    }

    picofs_refresh_files();
    
    return(err);
}

int picofs_append_to_flash(char *dst, size_t dst_len, char *src, size_t src_len)
{
    static char *current_destination = NULL;
    static char page_buffer[256];
    static int page_index = 0;
    static size_t total_bytes = 0;
    int src_index = 0;

    if (!dst)
    {
        printf("NULL dst indicating flush page buffer\n");
    }
    else if (((int)(dst - FS_START))%256)
    {
        printf("MASSIVE ERROR! Consolidaton area is not page aligned %p\n", dst);
    }
    else
    {
        printf("Good consolidation area %p\n", dst);
    }

    if (dst && (dst != current_destination))
    {
        // start new consolidation
        page_index = 0;
        total_bytes = 0;
        current_destination = dst;

        // accumulate and write flash once page ready
        for (src_index = 0; src_index < src_len; src_index++)
        {
            page_buffer[page_index] = src[src_index];

            if (++page_index == 256)
            {
                // program page
                //memcpy(dst+total_bytes, page_buffer, 256); 
                picofs_flash_program(dst+total_bytes, page_buffer, 256); 
                page_index = 0;
                total_bytes += 256;               
            }
        }        
    }
    else if (dst && (dst == current_destination))
    {
        // accumulate and write flash once page ready
        for (src_index = 0; src_index < src_len; src_index++)
        {
            page_buffer[page_index] = src[src_index];

            if (++page_index == 256)
            {
                // program page
                //memcpy(dst+total_bytes, page_buffer, 256);
                picofs_flash_program(dst+total_bytes, page_buffer, 256);  
                page_index = 0;
                total_bytes += 256;               
            }
        }
    }
    else
    {
        if (current_destination)
        {
            // end of files -- flush remaining data to flash
            printf("flushing partially filled page with %d bytes of data\n", page_index);
            for (; page_index < 256; page_index++)
            {
                page_buffer[page_index] = FS_ERASED_CELL_VALUE;
            }

            // program page
            //memcpy(current_destination + total_bytes, page_buffer, 256);
            picofs_flash_program(current_destination+total_bytes, page_buffer, 256); 
            page_index = 0;
            total_bytes += 256; 
            current_destination = NULL;            
        }
    }


    return(0);
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

    memset(consoldation_files, 0, sizeof(consoldation_files));
    memset(buffer, FS_ERASED_CELL_VALUE, len); 

    while(!picofs_iter_next_file(&current, false))
    {
        if (current)
        {
            consoldation_files[current->file_id].valid = true;
            num_files++;
            size_files_plus_remnants += current->file_size;

            if (current->file_sequence >= consoldation_files[current->file_id].trailer->file_sequence)    // TODO proper handling of sequence wrap around!
            {
                consoldation_files[current->file_id].trailer = current;
            }
        }
        else
        {
            printf("picofs: error: next iter unexpectedly returned a NULL pointer without an error return value\n");
            break;
        }
    }

    qsort(consoldation_files, NUM_ROWS(consoldation_files), sizeof(FILE_STATUS_T), picofs_descending_size_compare);

    if (num_files)
    {
            picofs_printf("Size\t\tFID\tSEQ\tName\n");
    }

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (consoldation_files[i].valid && !consoldation_files[i].trailer->file_status)
        {
            picofs_printf("%08d\t%d\t%d\t%s\n", consoldation_files[i].trailer->file_size, consoldation_files[i].trailer->file_id, consoldation_files[i].trailer->file_sequence, consoldation_files[i].trailer->name);
            size_files += consoldation_files[i].trailer->file_size;
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
            if ((consoldation_files[i].valid && consoldation_files[i].trailer) &&
                (consoldation_files[i].trailer->file_size < (len - total_written)) &&
                (consoldation_files[i].trailer->file_id != exclude_fid) &&
                (consoldation_files[i].trailer->file_sequence < (FS_MAX_SEQ-1)))
            {
                // normal consolidation
                printf("copying...\n");
                hex_dump((u8_t *)((char *)consoldation_files[i].trailer + sizeof(FILE_TRAILER_T) - consoldation_files[i].trailer->file_size), consoldation_files[i].trailer->file_size);
                memcpy(consolidation_area + total_written, (char *)consoldation_files[i].trailer + sizeof(FILE_TRAILER_T) - consoldation_files[i].trailer->file_size, consoldation_files[i].trailer->file_size);
                picofs_increment_sequence((FILE_TRAILER_T *)(consolidation_area + total_written + consoldation_files[i].trailer->file_size - sizeof(FILE_TRAILER_T)));
                total_written += consoldation_files[i].trailer->file_size;
                shell_printf("picofs: consolidated file: %s %d bytes\n", consoldation_files[i].trailer->name, consoldation_files[i].trailer->file_size);
            }
            else if ((consoldation_files[i].valid && consoldation_files[i].trailer) &&
                     ((consoldation_files[i].trailer->file_size + sizeof(FILE_TRAILER_T)) < (len - total_written)) &&
                     (consoldation_files[i].trailer->file_id != exclude_fid) &&
                     (consoldation_files[i].trailer->file_sequence == (FS_MAX_SEQ-1)))
            {
                // special case: consolidation with FID rollover during consolidation, requires extra space for a trailer marking file deletion 
                printf("copying...\n");
                hex_dump((u8_t *)((char *)consoldation_files[i].trailer + sizeof(FILE_TRAILER_T) - consoldation_files[i].trailer->file_size), consoldation_files[i].trailer->file_size);
                memcpy(consolidation_area + total_written, (char *)consoldation_files[i].trailer + sizeof(FILE_TRAILER_T) - consoldation_files[i].trailer->file_size, consoldation_files[i].trailer->file_size);
                if (!picofs_increment_sequence((FILE_TRAILER_T *)(consolidation_area + total_written + consoldation_files[i].trailer->file_size - sizeof(FILE_TRAILER_T))))
                {
                    total_written += consoldation_files[i].trailer->file_size;
                    shell_printf("picofs: consolidated file with FID rollover: %s %d bytes\n", consoldation_files[i].trailer->name, consoldation_files[i].trailer->file_size);

                    // append empty file marking the deletion of the old FID that had run out of sequence numbers
                    memcpy(consolidation_area + total_written, (char *)consoldation_files[i].trailer, sizeof(FILE_TRAILER_T));
                    ((FILE_TRAILER_T *)(consolidation_area + total_written))->file_status = 1;                     // mark for deletion
                    ((FILE_TRAILER_T *)(consolidation_area + total_written))->file_size = sizeof(FILE_TRAILER_T);  // empty file
                    ((FILE_TRAILER_T *)(consolidation_area + total_written))->file_sequence = FS_MAX_SEQ;          // last sequence
                    ((FILE_TRAILER_T *)(consolidation_area + total_written))->crc = 0;                             // CRC of an empty file is zero
                    total_written += sizeof(FILE_TRAILER_T);
                }
                else
                {
                    printf("Skipped file for speculative consolidation because we ran out of FIDs to rollover a file with sequence 254\n");
                }
            }            
            else
            {
                if (consoldation_files[i].valid)
                {
                    printf("Skipped file for speculative consolidation because: size %d vs %d | fid %d vs %d | ptr %p | seq %d\n", consoldation_files[i].trailer->file_size, (len - total_written), consoldation_files[i].trailer->file_id, exclude_fid, consoldation_files[i].trailer,  consoldation_files[i].trailer->file_sequence);
                }
            }

            if(total_written >= len)
            {
                printf("picofs: consolidate: reached total consolidated size of %d bytes @ FID %d\n", len, consoldation_files[i].trailer->file_id);
                break;
            }
        }
    }

    shell_printf("picofs: consolidation completed total size is %d bytes\n", total_written);

    return(0);
}