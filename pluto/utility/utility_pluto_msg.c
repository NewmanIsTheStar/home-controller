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



/*!
 * \brief Send a pluto command
 * 
 * \return 0 on success, -1 on error
 */
int send_pluto_message(char *message)
{
    //char timestamp[50];
    char tx_buffer[200];
    int sent_bytes = -1;
    static int pluto_socket = -1;
    //static struct sockaddr_in pluto_address;



    // (re)establish socket connection
    if (pluto_socket < 0) pluto_socket = establish_socket("127.0.0.1", /*&pluto_address,*/ 6969, SOCK_DGRAM);

    if (pluto_socket >= 0)
    {
        snprintf(tx_buffer, sizeof(tx_buffer), "Nice!%s", message);
        printf("Sending pluto %s\n", tx_buffer);
        sent_bytes = send(pluto_socket, tx_buffer, strlen(tx_buffer), 0);              

        if (sent_bytes < 0)
        {
            lwip_close(pluto_socket);
            pluto_socket = -1;
            web.pluto_transmit_failures++;
        }  

    }  

    return(sent_bytes);
}
