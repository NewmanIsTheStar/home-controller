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
extern FILE_METRICS_T picofs_metrics[FS_NUM_FID];

//static variables


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
    p = FS_END - 1 - sizeof(FILE_TRAILER_T);

    // scan flash
    while (((char *)p) >= FS_START)
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
int picofs_list_all_files(void)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *current = NULL;
    int num_files = 0;
    int size_files = 0;
    int size_files_plus_remnants = 0;  // remnants include deleted files and old versions of files that are no longer visible but are taking up space in flash
    u8_t *consolidation_area = NULL;
    size_t consolidation_area_size = 0;
    u32_t calculated_crc = 0;

    memset(picofs_metrics, 0, sizeof(picofs_metrics));

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            picofs_metrics[current->file_id].valid = true;
            num_files++;
            size_files_plus_remnants += current->file_size;

            if (current->file_sequence >= picofs_metrics[current->file_id].trailer->file_sequence)
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
            picofs_printf("Size\t\tFID\tSEQ\tCRC calc\tCRC file\tName\n");
    }

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (picofs_metrics[i].valid && !picofs_metrics[i].trailer->file_status)
        {

            // calculate crc
            calculated_crc = calculate_crc32_universal_unaligned_rtos(((const uint8_t *)(picofs_metrics[i].trailer)) + sizeof(FILE_TRAILER_T) - picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_size - sizeof(FILE_TRAILER_T));
            picofs_printf("%08d\t%d\t%d\t%08x\t%08x\t%-16s\n", picofs_metrics[i].trailer->file_size, picofs_metrics[i].trailer->file_id, picofs_metrics[i].trailer->file_sequence, calculated_crc, picofs_metrics[i].trailer->crc, picofs_metrics[i].trailer->name);

            size_files += picofs_metrics[i].trailer->file_size;
        }
    }

    picofs_printf("\nTotal size    %08d\n", size_files);
    picofs_printf("Remnants size %08d\n", size_files_plus_remnants - size_files);

    picofs_printf("Space to consolidate? %s\n", picofs_find_contiguous_free_area(size_files, &consolidation_area, &consolidation_area_size)?"NO":"YES");




    return(0);
}

