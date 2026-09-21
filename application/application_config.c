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
#include "system_config.h"
#include "syscfg.h"

//#define DISABLE_SYSCFG_UPGRADE

NON_VOL_VARIABLES_T *cfg = NULL;

// system configuration conversion functions
void appcfg_blank_to_v1(void *previous_config);

// table of conversion functions -- these are run sequentially from the starting version to convert to the latest version
SYSTEM_CONVERSION_T appcfg_info[] =
{
    {1,      sizeof(NON_VOL_VARIABLES_T),  offsetof(NON_VOL_VARIABLES_T, version),   offsetof(NON_VOL_VARIABLES_T, crc),   &appcfg_blank_to_v1},                 
};

int appcfg_info_rows = NUM_ROWS(appcfg_info);



/*!
 * \brief Set default values for configuration v1
 * 
 * \return 0 on success, -1 on error
 */
void appcfg_blank_to_v1(void *previous_config)
{
    int i;

    if (cfg)
    {
        printf("Initializing applicaton configuration version 1 @ %p\n", cfg);
        
        memset((char *)cfg, 0, sizeof(NON_VOL_VARIABLES_T));

        // version
        cfg->version = 1;
        
        //home controller
        cfg->hc_enable = 1;
        
        printf("config_blank_to_v1: setting all automations to undefined\n");
        for(i=0; i<NUM_ROWS(cfg->automation_name); i++)
        {
            cfg->automation_state[i] = AUTOMATION_UNDEFINED;
            sprintf(cfg->automation_name[i], "automation%02d", i);
            cfg->automation_triggered[i] = 0;
        }        
    }
    else
    {
        printf("Failed to initialize applicaiton configruation -- NULL pointer\n");
    }
}



