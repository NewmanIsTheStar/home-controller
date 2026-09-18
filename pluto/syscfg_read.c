/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 // be cautious what you include here lwip and fcntl have some important conflicts related to BSD / sockets
#include <stdio.h>
#include <stdlib.h>
//#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <lwip/arch.h>
#include "picofs.h"
#include "config.h"


#include <sys/stat.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "syscfg.h"
#include "pluto.h"
#include "utility.h"

#include "flash.h"
#include "picofs.h"


//#define DISABLE_SYSCFG_VALIDATION (1)


/*!
 * \brief Copy the configuration from flash into RAM.  Set default values if flash is corrupt.
 * 
 * \return 0 on success, -1 on error
 */
//int syscfg_read(char *filename, SYSTEM_CONVERSION_T conversion_table[], int conversion_table_rows, void **configuration_buffer)
int syscfg_read(void)
{
    int err = 0;

#ifndef DISABLE_SYSCFG_VALIDATION
    // read configuration and upgrade to the latest version if necessary
    err = syscfg_validate(); 
#else
    // read configuration from flash
    err = syscfg_map_file();  
    printf("Warning: Configuration validation disabled. Using whatever random garbage happens to be in flash...\n");       
#endif

    return(err);
}

