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
// u8_t picofs_list_files_within_size_range(int size_lo, int size_hi, u8_t *file_id_list, int *file_size_list, int list_len);
// u8_t picofs_find_file_at_location(char *search, FILE_TRAILER_T **trailer);

// int picofs_refresh_metrics(void);
// int picofs_ascending_size_compare(const void *a, const void *b);
// int picofs_descending_size_compare(const void *a, const void *b);
// u32_t picofs_get_start_sector(FILE_TRAILER_T *trailer);
// u32_t picofs_get_end_sector(FILE_TRAILER_T *trailer);
// int picofs_append_to_flash(char *dst, size_t dst_len, char *src, size_t src_len);
// bool picofs_deleted_file_has_remnants_in_other_sectors(FILE_TRAILER_T *deleted_file);
// bool picofs_deleted_file_ready_for_erasure(FILE_TRAILER_T *candidate_file);


// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables



int picofs_read(int fd, char *ptr, int len)
{
    int i;

    //printf("read start offset = %d\n", custom_fds[fd].data_offset);

    for(i=0; i<len; i++)
    {       
        if ((custom_fds[fd].data_offset + i) < custom_fds[fd].data_len)
        {
            ptr[i] = custom_fds[fd].data[custom_fds[fd].data_offset + i];
        }
        else
        {
            //printf("truncated read @ offset = %d i = %d\n", custom_fds[fd].data_offset, i);
            break;
        }
    }
    
    //hex_dump(ptr, i);
    
    custom_fds[fd].data_offset += i;

    //printf("new offset = %d\n", custom_fds[fd].data_offset);

    return(i);
}

