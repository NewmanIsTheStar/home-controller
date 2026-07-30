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


//static variables
FILE_TEST_T test_filesystem[40];


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
        test_filesystem[0].test_trailer.crc = 0xdd424c0e;  // 0e 4c 42 dd
        STRNCPY(test_filesystem[0].test_trailer.name, "elephant", sizeof("elephant"));  
        // STRNCPY(test_filesystem[0].test_data, "This is test file A.", sizeof("This is test file A.")); 
        
        test_filesystem[0].test_data[0] = 0;
        for (i=0; i<224; i++)
        {
            STRNCAT(test_filesystem[0].test_data, "A", sizeof(test_filesystem[0].test_data));
        }

        // TEST TEST TEST
        // inject corruption into file!
        test_filesystem[0].test_data[23] = 'X';
        
        test_filesystem[0].test_trailer.file_size = sizeof(FILE_TRAILER_T) + strlen(test_filesystem[0].test_data) + 1;
        // STRNCPY(test_filesystem[0].test_trailer.magic_number, "spf", sizeof(test_filesystem[0].test_trailer.magic_number));
        // test_filesystem[0].test_trailer.crc = 0;

        STRNCPY(test_filesystem[1].test_trailer.magic_number, "pfs", sizeof(test_filesystem[1].test_trailer.magic_number));
        test_filesystem[1].test_trailer.picofs_version = 0;
        test_filesystem[1].test_trailer.file_id = 1;    
        test_filesystem[1].test_trailer.file_sequence = 253;  
        test_filesystem[1].test_trailer.file_status = 0; 
        test_filesystem[1].test_trailer.file_size = 0; 
        test_filesystem[1].test_trailer.crc = 0xc9fd01f3;  // f3 01 fd c9
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
    
        STRNCPY(test_filesystem[2].test_trailer.magic_number, "pfs", sizeof(test_filesystem[2].test_trailer.magic_number));
        test_filesystem[2].test_trailer.picofs_version = 0;
        test_filesystem[2].test_trailer.file_id = 2;    
        test_filesystem[2].test_trailer.file_sequence = 0;  
        test_filesystem[2].test_trailer.file_status = 0; 
        test_filesystem[2].test_trailer.file_size = 0; 
        test_filesystem[2].test_trailer.crc = 0x5af465bc; 
        STRNCPY(test_filesystem[2].test_trailer.name, "goat", sizeof("goat"));
        test_filesystem[2].test_data[0] = 0;
        
        memset((void *)test_filesystem[2].test_data, 0, 224);
        STRNCAT(test_filesystem[2].test_data, "10 FOR X = 0 TO 3\n20 FOR Y = 0 TO 4095\n30 PRINT X;\" \";Y\n40 NEXT\n50 NEXT\n", sizeof(test_filesystem[2].test_data));
        test_filesystem[2].test_trailer.file_size = 256; 


        test_init = true;
    }

    return (0);
}
