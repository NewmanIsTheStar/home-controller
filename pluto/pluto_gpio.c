/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lwip/opt.h"
#if !LWIP_HTTPD_SUPPORT_WEBSOCKET
#error "LWIP_HTTPD_SUPPORT_WEBSOCKET must be enabled in lwipopts.h to use this file!"
#endif

#include <sys/stat.h> 
#include "pico/cyw43_arch.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"
#include "hardware/structs/powman.h"
#include "hardware/regs/powman.h"
#include <hardware/flash.h>
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cgi.h"
#include "utility.h"
#include "config.h"
#include "application_config.h"
#include "watchdog.h"
#include "worker_tasks.h"
#include "pluto_wifi_country.h"
#include "pluto.h"
#include "picofs.h"
#include "pluto_swload.h"
#include "calendar.h"
#include "system_web.h"
#include "ssi.h"



/*!
 * \brief GPIO defaults for pins that are unused but connected to hardware
 *
 * \param none
 *
 * \return 0
 */
int set_gpio_defaults(void)
{
    int i;

    for(i=0; i<NUM_ROWS(sys->gpio_default); i++)
    {
        switch(sys->gpio_default[i])
        {
        default:
        case GP_UNINITIALIZED:
            break;
        case GP_INPUT_FLOATING:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            printf("GPIO %d :: input floating\n", i);            
            break;
        case GP_INPUT_PULLED_HIGH:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            gpio_set_pulls (i, true, false);
            printf("GPIO %d :: input internally pulled high\n", i);             
            break;
        case GP_INPUT_PULLED_LOW:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            gpio_set_pulls (i, false, true);
            printf("GPIO %d :: input internally pulled low\n", i);             
            break;
        case GP_OUTPUT_HIGH:
            gpio_init(i);
            gpio_set_dir (i, true);
            gpio_put (i, true);
            printf("GPIO %d :: output high\n", i); 
            break;
        case GP_OUTPUT_LOW:
            gpio_init(i);
            gpio_set_dir (i, true);
            gpio_put (i, false);
            printf("GPIO %d :: output low\n", i);
            break;            
        }
    }
}
