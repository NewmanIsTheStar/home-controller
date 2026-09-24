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
 * \brief Send a syslog message
 *
 * \param[in]   log_name      name of log file on server
 * \param[in]   format, ...   variable parameters printf style  
 * 
 * \return num bytes sent or -1 on error
 */
int send_syslog_message(char *log_name, const char *format, ...)
{
    static int syslog_socket = -1;
    //tatic struct sockaddr_in syslog_address;
    static char ip_address_string[50] = "";  
    int message_chars_remaining = 0;
    int sent_bytes = -1;  
    va_list args;
    char timestamp[50];
    char syslog_message[200];


    if (sys->syslog_enable)
    {
        // cache our ip address for use in syslog messages 
        if (!*ip_address_string) STRNCPY(ip_address_string, ipaddr_ntoa(netif_ip4_addr(&cyw43_state.netif[0])), sizeof(ip_address_string));

        // (re)establish socket connection
        if (syslog_socket < 0) syslog_socket = establish_socket(sys->syslog_server_ip, /*&syslog_address,*/ 514, SOCK_DGRAM);    

        if (syslog_socket >= 0)
        {
            if (!get_timestamp(timestamp, sizeof(timestamp), true, true))
            {   
                message_chars_remaining = sizeof(syslog_message);
                snprintf(syslog_message, message_chars_remaining, "<165>1 %s %s %s 1 - - %%%% ", timestamp, ip_address_string, log_name);

                message_chars_remaining = sizeof(syslog_message) - strlen(syslog_message);
                va_start(args, format);  
                vsnprintf(syslog_message+strlen(syslog_message), message_chars_remaining, format, args); 
                va_end(args); 
                syslog_message[199] = 0;  // ensure string terminated

                //cyw43_arch_lwip_begin();
                sent_bytes = send(syslog_socket, syslog_message, strlen(syslog_message), 0);
                //cyw43_arch_lwip_end();
                //printf("sent %d bytes.  MSG: %s\n", sent_bytes, syslog_message); 

                if (sent_bytes < 0)
                {
                    //cyw43_arch_lwip_begin();
                    close(syslog_socket);
                    //cyw43_arch_lwip_end();
                    syslog_socket = -1;
                    web.syslog_transmit_failures++;
                }          
            }
        }
    }
    return(sent_bytes);
}