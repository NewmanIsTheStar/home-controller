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

//static variables
// FILE_TEST_T test_filesystem[10];
 
// #define FS_FLASH_START ((char *)(&test_filesystem))
// #define FS_FLASH_END ((char *)(&test_filesystem) + sizeof(test_filesystem))
// #define FS_VERION (0)


/*
name: A pointer to a null-terminated string specifying the path to the file you want to open.
flags: A bitwise OR mask (|) determining the file access mode and operational behaviors.
mode: An optional argument (typically an octal number or permission macros) used only when a new file is being created (via O_CREAT or O_TMPFILE). 
If neither flag is provided, this parameter is ignored.

Common Flags (flags)The access mode must include exactly one of the following core options:
O_RDONLY: Open for reading only.
O_WRONLY: Open for writing only.
O_RDWR: Open for both reading and writing.

You can bitwise OR (|) these with additional file creation or status flags:
O_CREAT: Create the file if it does not exist.
O_TRUNC: Truncate the file length to 0 if it already exists and is opened for writing.
O_APPEND: Move the file offset pointer to the end of the file before every write.
O_EXCL: Ensure that this call creates the file; if the file already exists, the call fails (used alongside O_CREAT).
*/
picofs_open(&custom_fds[fd].my_fs_handle, name, flags