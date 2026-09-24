/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include <hardware/flash.h>

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"


#include "stdarg.h"

//#include "weather.h"

#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"
#include "watchdog.h"
#include "pluto.h"

//prototype

// external variables
extern APP_CONFIG_T config;
extern WEB_VARIABLES_T web;

//global


/*!
 * \brief Initialize the GPIO pin used to control the irrigation relay
 * 
 * \return 0 on success, non-zero on failure
 */
int initialize_relay_gpio(int gpio_number)
{
    int err = -1;

    switch(gpio_number)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 26:
    case 27:
    case 28:
        gpio_init(gpio_number);
        gpio_set_dir(gpio_number, GPIO_OUT); 
        err = 0;  
        break;
    default:
        printf("Rejected attempt to initialize invalid GPIO (%d).\n", gpio_number);
        err = -2;
        break;
    }
    return(err);
}

/*!
 * \brief Check if GPIO is valid
 * 
 * \return true if valid
 */
bool gpio_valid(int gpio_number)
{
    bool valid = false;

    switch(gpio_number)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 26:
    case 27:
    case 28: 
        valid = true;  
        break;
    default:
        valid = false;
        break;
    }
    return(valid);
}

/*!
 * \brief Check for GPIO conflict
 * 
 * \return true if valid
 */
bool gpio_conflict(int *gpio_list, int len)
{
    int x,y;
    bool conflict = false;

    for (x=0; x++; x<len)
    {
        for(y=0; y++; y<len)
        {
            if (x != y)
            {
                if (gpio_valid(gpio_list[x]) && (gpio_list[x] == gpio_list[y]))
                {
                    conflict = true;
                    break;
                }
            }
        }
    }

    return(conflict);
}

/*!
 * \brief Get i2c block corresponding to the gpio pins
 * 
 * \return true if valid
 */
i2c_inst_t *gpio_get_i2c(int gpio_clock, int gpio_data)
{
    i2c_inst_t *i2c_block[2] = {NULL, NULL};
    int gpio_to_check[2] = {-1, -1};
    int i;

    gpio_to_check[0] = gpio_clock;
    gpio_to_check[1] = gpio_data;    

    for(i=0; i<2; i++)
    {
        switch(gpio_to_check[i])
        {
        // i2c block 0
        case 0:
        case 1:
        case 4:
        case 5:
        case 8:
        case 9:
        case 12:
        case 13:
        case 16:
        case 17:
        case 20:
        case 21:
            i2c_block[i] = i2c0;
            break;
        // i2c block 1
        case 2:
        case 3:
        case 6:
        case 7:
        case 10:
        case 11:
        case 14:
        case 15:
        case 18:
        case 19:
        case 22:
        case 26:
        case 27:
            i2c_block[i] = i2c1;
            break;
        default:
        case 28: 
            i2c_block[i]= NULL;
            break;

        }
    }   

    if (i2c_block[0] != i2c_block[1])
    {
        i2c_block[0] = NULL;
    }

    return(i2c_block[0]);
}

