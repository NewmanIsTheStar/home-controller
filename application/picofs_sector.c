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
 * \brief erase obsolete sectors
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_erase_obsolete_sectors(void)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *current = NULL;
    FILE_TRAILER_T *look_ahead = NULL;
    int num_files = 0;
    int size_files = 0;
    int size_files_plus_remnants = 0;  // remnants include deleted files and old versions of files that are no longer visible but are taking up space in flash
    u8_t *consolidation_area = NULL;
    u32_t file_start_sector = 0;
    u32_t file_end_sector = 0;
    bool erasure_possible = false;
    u32_t last_valid_start_sector = UINT_MAX;

    picofs_refresh_metrics();

    while(!picofs_iter_next_file(&current))
    {
        if (current)
        {
            printf("checking %s seq %d\n", current->name, current->file_sequence);

            if ((picofs_metrics[current->file_id].trailer != current) || picofs_deleted_file_ready_for_erasure(current))
            {

                file_start_sector = picofs_get_start_sector(current);
                file_end_sector = picofs_get_end_sector(current);
                
                printf("--found remnant of file %s start sector %d to end sector %d\n", current->name, file_start_sector, file_end_sector);

                if (file_end_sector != last_valid_start_sector)
                {
                    erasure_possible = true;
                    look_ahead = current;

                    while(!picofs_iter_next_file(&look_ahead))
                    {
                        if (picofs_get_end_sector(look_ahead) == file_start_sector)
                        {
                            if ((picofs_metrics[look_ahead->file_id].trailer != look_ahead) || picofs_deleted_file_ready_for_erasure(look_ahead))
                            {
                                printf("---lookahead found adjacent obsolete file in the same sector %s SEQ %d\n", look_ahead->name, look_ahead->file_sequence);
                                // adjacent obsolete file so expand range to cover it  
                                file_start_sector = picofs_get_start_sector(look_ahead); 
                                current = look_ahead;
                            }
                            else
                            {
                                printf("---lookahead found adjacent valid file in the same sector %s SEQ %d\n", look_ahead->name, look_ahead->file_sequence);
                                // adjacent valid file so we cannot erase blocks that contain the valid file
                                if (file_end_sector > file_start_sector)
                                {
                                    // eliminate sector that contains both files from erasure range
                                    file_start_sector++;

                                    printf("----file start sector removed from consideration. new start sector to evaluate is %d\n", file_start_sector);

                                    //TODO -- need to handle possibility / prevent creating a hole in file but leaving the trailer e.g. new file insert in hole and iterator will skip over it!
                                }
                                else
                                {
                                    // no erasure possible because first sector of obsolete file also contains a valid file
                                    erasure_possible = false;

                                    printf("----erasure not possible because first sector also conatins a valid file\n");
                                }
                                break;
                            }
                        }
                        else
                        {
                            printf("---next file end sector and current file start sector are not the same so no conflict can occur to prevent erasure\n");
                            
                            break;
                        }
                    }

                    if (erasure_possible)
                    {
                        if (file_start_sector == file_end_sector)
                        {
                            printf("--ERASING Sector %d\n", file_start_sector);
                            shell_printf("--ERASING Sector %d\n", file_start_sector);
                        }
                        else
                        {
                            printf("--ERASING Sectors %d to %d\n", file_start_sector, file_end_sector);
                            shell_printf("--ERASING Sectors %d to %d\n", file_start_sector, file_end_sector);
                        }
                        
                        picofs_flash_erase_sector_range(file_start_sector, file_end_sector);
                    }
                }
            }
            else
            {
                last_valid_start_sector = picofs_get_start_sector(current);
                printf("--found valid file with start sector %d\n", last_valid_start_sector);
            }
        }
    }

    // TODO:  files with status 1 (deleted) become eligible for erasre once all prior sequence numbers have been erased and NOT before!!!
    
    return(0);
}

/*!
 * \brief check if sector is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  trailer      pointer to file trailer
 *  *     
 * \return 0 on success
 */
u32_t picofs_get_start_sector(FILE_TRAILER_T *trailer)
{
    u32_t start_block;
    
    start_block = ((((char *)trailer + sizeof(FILE_TRAILER_T)) - trailer->file_size) - FS_START)/FS_SECTOR_SIZE; 

    return(start_block);
}

/*!
 * \brief check if sector is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  trailer      pointer to file trailer
 *  *     
 * \return 0 on success
 */
u32_t picofs_get_end_sector(FILE_TRAILER_T *trailer)
{
    u32_t end_block;
    
    end_block = (((char *)trailer + sizeof(FILE_TRAILER_T) - 1) - FS_START)/FS_SECTOR_SIZE; 

    return(end_block);
}