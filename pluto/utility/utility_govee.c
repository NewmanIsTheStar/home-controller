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

#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"
#include "watchdog.h"
#include "pluto.h"


//prototype

// external variables
extern WEB_VARIABLES_T web;

//global



#ifdef USURPER
/*!
 * \brief Send a govee command
 *
 * \param[in]   on 1=on, 0=off  
 * 
 * \return 0 on success, non-zero on error
 */
int send_govee_command(int on, int red, int green, int blue)
{
    //char timestamp[50];
    char log_message[200];
    int sent_bytes = -1;
    static int govee_socket = -1;
    //static struct sockaddr_in govee_address;
    //static int govee_multicast_socket = -1;
    //static struct sockaddr_in govee_multicast_address;    
    //fd_set readset;
    //struct timeval tv;  
    //int retry = 0;
    //int ret = 0;
    //int read_bytes = 0;
    //unsigned char rx_buffer[128];  
    //int p;

    // (re)establish socket connection
    if (govee_socket < 0) govee_socket = establish_socket(cfg->govee_light_ip, /*&govee_address,*/ 4003, SOCK_DGRAM);
    //if (govee_multicast_socket < 0) govee_multicast_socket = establish_multicast_socket(&govee_multicast_address, 4002, SOCK_DGRAM);    


    if (govee_socket >= 0)
    {
        snprintf(log_message, sizeof(log_message), "{\"msg\":{\"cmd\":\"colorwc\",\"data\":{\"color\":{\"r\":%d,\"g\":%d,\"b\":%d},\"colorTemInKelvin\":0}}}\n", red, green, blue);
        //printf("Sending govee: %s\n", log_message);
        sent_bytes = send(govee_socket, log_message, strlen(log_message), 0);              

        if (sent_bytes == 0)
        {
            lwip_close(govee_socket);
            govee_socket = -1;
            web.govee_transmit_failures++;
        }  

    }  

    if (govee_socket >= 0)
    {
        //for (p=0; p<10; p++){
        // snprintf(log_message, sizeof(log_message), "{\"msg\":{\"cmd\":\"devStatus\",\"data\":{}}}\n");
        // printf("Sending govee: %s\n", log_message);
        // sent_bytes = send(govee_socket, log_message, strlen(log_message), 0);              

        // if (sent_bytes < 0)
        // {
        //     lwip_close(govee_socket);
        //     govee_socket = -1;
        // }  
        // else if (sent_bytes > 0)
        // {
        //     printf("read back govee state\n");                    
        //     for (retry=0; retry<5; retry++)
        //     {
        //         FD_ZERO(&readset);
        //         FD_SET(govee_multicast_socket, &readset);
        //         tv.tv_sec = 2;
        //         tv.tv_usec = 500;

        //         printf("calling select...\n");
        //         watchdog_pulse();
        //         ret = select(govee_multicast_socket + 1, &readset, NULL, NULL, &tv);
        //         watchdog_pulse();
        //         perror("select ::");
        //         printf("returned from select with value %d\n", ret);
                

        //         if ((ret > 0) && FD_ISSET(govee_multicast_socket, &readset))
        //         {
        //             printf("calling recv...\n");
        //             read_bytes = recv(govee_multicast_socket, rx_buffer, 128, 0);
        //             printf("...returned from recv with value %d\n", read_bytes);
        //             if (read_bytes > 0)
        //             {
        //                 hex_dump(rx_buffer, read_bytes);
        //                 //TODO: function to parse govee status message
        //             }
        //             else {
        //                 perror("READ ERROR = ");
        //                 printf("read returned %d  ||| retry = %d\n", read_bytes, retry);
        //                 err = -1;
        //             }
        //         }
        //         else
        //         {
        //             printf("select returned %d  and FD_ISSET was not set  ||| retry = %d\n", ret, retry);
        //             err = -1;
        //         }  

        //     }         
        // }
        //else
        // {
        //     printf("sent_bytes = %d\n", sent_bytes);

        //     // close socket
        //     lwip_close(govee_socket);
        //     govee_socket = -1;
        //     err = -1;
        // }
        //SLEEP_MS(100);
        //} // for p -- debug loop
    }

    return(sent_bytes);
}



/*!
 * \brief Send a govee command
 *
 * \param[in]   on 1=on, 0=off  
 * 
 * \return 0 on success, -1 on error
 */
int check_govee_state(void)
{
    //char timestamp[50];
    char log_message[200];
    int sent_bytes = -1;
    static int govee_socket = -1;
    //static struct sockaddr_in govee_address;

    // (re)establish socket connection
    if (govee_socket < 0) govee_socket = establish_socket(cfg->govee_light_ip, /*&govee_address,*/ 4003, SOCK_DGRAM);

    if (govee_socket >= 0)
    {
        snprintf(log_message, sizeof(log_message), "{\"msg\":{\"cmd\":\"devStatus\",\"data\":{}}}\n");
        //printf("Sending govee: %s\n", log_message);
        sent_bytes = send(govee_socket, log_message, strlen(log_message), 0);              

        if (sent_bytes < 0)
        {
            lwip_close(govee_socket);
            govee_socket = -1;
            web.govee_transmit_failures++;
        }  

    }    

    return(sent_bytes);
}
#endif