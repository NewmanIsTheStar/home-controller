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
 * \brief Set value of double buffered integer - crude alternative to using semaphores / message queues
 * 
 * \return 0 on success, non-zero on error
 */
int set_double_buf_integer(DOUBLE_BUF_INT *integer, int value)
{
    int ierr = 0;

    integer->lock++;

    integer->write_data = value;

    integer->lock--;

    return (ierr);
}

/*!
 * \brief Get value of double buffered integer - crude alternative to using semaphores / message queues
 * 
 * \return integer value -- if repeated collisions occur we return the last known value
 */
int get_double_buf_integer(DOUBLE_BUF_INT *integer, int retry)
{
    int temp;
    
    do 
    {
        if (!integer->lock)
        {
            temp = integer->write_data;

            // check for corruption by a task switch
            if (temp == integer->write_data)
            {
                integer->read_data = temp;
                break;
            }
        }
        
        if(retry > 0)
        {
            sleep_us(1);
        }
    }
    while (retry-- > 0);

    return (integer->read_data);
}

