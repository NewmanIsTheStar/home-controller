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
#include "syscfg.h"
#include "system_config.h"
#include "application_config.h"


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
#include "syscfg.h"
#include "system_config.h"
#include "application_config.h"


//#define DISABLE_SYSCFG_VALIDATION (1)

extern SYSTEM_CONVERSION_T syscfg_info[];
extern int syscfg_info_rows;
extern SYSTEM_CONVERSION_T appcfg_info[];
extern int appcfg_info_rows;

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
    err += syscfg_validate("system.cfg", syscfg_info, syscfg_info_rows, (void **)&sys);               //TODO: use a special segment to automatically register/discover multiple configs at build time (remove hard dependices in this source file)
    err += syscfg_validate("application.cfg", appcfg_info, appcfg_info_rows, (void **)&cfg);     
#else
    // read configuration from flash
    //err = syscfg_map_file();  
    err += syscfg_mmap("system.cfg", (void **)&sys);
    err += syscfg_mmap("applicaiton.cfg", (void **)&cfg);

    printf("Warning: Configuration validation disabled. Using whatever random garbage happens to be in flash...\n");       
#endif

    if ((!sys) || (!cfg))
    {
        printf("Guru Meditation: failed to allocate memory for the configuration.\n");
        for(;;)
        {
            SLEEP_MS(60000); 
        }
    }

    return(err);
}

