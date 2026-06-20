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

//static variables
FILE_HEADER_T test[10];
#define FS_FLASH_START ((char *)(&test))
#define FS_FLASH_END ((char *)(&test) + sizeof(test))
#define FS_VERION (0)

/*!
 * \brief Get pointer to file header matching passed filename
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  header       pointer to file header
 *  *     
 * \return 0 on success
 */
int picofs_find_by_name(char *filename, char **header)
{
    int err = -1;
    u8_t *p = FS_FLASH_START;
    //  u8_t *next = NULL; 
    FILE_HEADER_T *h = NULL;
    u8_t best_sequence = 0;
    bool first_sequnce = false;

    // TEST TEST TEST
    picofs_load_test_data();

    while (((char *)p) < FS_FLASH_END)
    {
        h = (FILE_HEADER_T *)p;

        if ((strncmp(h->magic_number, "pfs", 4) == 0) &&
            (h->picofs_version == FS_VERION) &&
            (strcmp(h->name, filename) == 0))
        {
            // match
            if (!first_sequnce)
            {
                best_sequence = h->file_sequence;
                *header = (char *)h;
                err = 0;
                p = p + sizeof(FILE_HEADER_T) + h->file_size + h->file_padding;
            }
            else
            {
                if ((h->file_sequence - best_sequence) < 128)
                {
                    best_sequence = h->file_sequence;
                    *header = (char *)h;
                    err = 0;
                    p = p + sizeof(FILE_HEADER_T) + h->file_size + h->file_padding;                    
                }
            }
        }

        if (p == ((u8_t *)h))
        {
            p++;
        }
    }


    return(err);
}


int picofs_load_test_data(void)
{
    STRNCPY(test[0].magic_number, "pfs", sizeof(test[0].magic_number));
    test[0].picofs_version = 0;
    test[0].file_id = 0;    
    test[0].file_sequence = 0;  
    test[0].file_padding = 0; 
    test[0].file_size = 0; 
    test[0].crc = 0;
    STRNCPY(test[0].name, "elephant", sizeof("elephant"));  

    STRNCPY(test[1].magic_number, "pfs", sizeof(test[1].magic_number));
    test[1].picofs_version = 0;
    test[1].file_id = 0;    
    test[1].file_sequence = 1;  
    test[1].file_padding = 0; 
    test[1].file_size = 0; 
    test[10].crc = 0;
    STRNCPY(test[1].name, "elephant", sizeof("elephant")); 

    return (0);
}

// typedef struct file_header
// {
//     u8_t magic_number[4];
//     u8_t picofs_version;
//     u8_t file_id;     
//     u8_t file_sequence;
//     u8_t file_padding;
//     u32_t file_size;        
//     u32_t crc;
//     char name[16];
// } FILE_HEADER_T;