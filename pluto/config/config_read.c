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

#include "config.h"
#include "pluto.h"
#include "utility.h"


#include "picofs.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"


//#define DISABLE_CONFIG_VALIDATION (1)

//TODO: use a special segment to automatically register/discover multiple configs at build time (remove hard dependices in this source file)
extern CONFIG_CONVERSION_T syscfg_info[];
extern int syscfg_info_rows;
extern CONFIG_CONVERSION_T appcfg_info[];
extern int appcfg_info_rows;

/*!
 * \brief Copy the configuration from flash into RAM.  Set default values if flash is corrupt.
 * 
 * \return 0 on success, -1 on error
 */
//int config_read(char *filename, CONFIG_CONVERSION_T conversion_table[], int conversion_table_rows, void **configuration_buffer)
int config_read(void)
{
    int err = 0;

#ifndef DISABLE_CONFIG_VALIDATION
    // read configuration and upgrade to the latest version if necessary
    err += config_validate("system.cfg", syscfg_info, syscfg_info_rows, (void **)&sys);               
    err += config_validate("application.cfg", appcfg_info, appcfg_info_rows, (void **)&cfg);     
#else
    // read configuration from flash
    //err = config_map_file();  
    err += config_mmap("system.cfg", (void **)&sys);
    err += config_mmap("applicaiton.cfg", (void **)&cfg);

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

