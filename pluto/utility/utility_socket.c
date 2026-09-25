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
int get_socket(char *address_string, int port, int type);

// external variables
extern APP_CONFIG_T config;
extern WEB_VARIABLES_T web;

//global
ip_addr_t dns_cache_response;  //temporary need per thread/ per call variable


/*!
 * \brief Create a TCP or UDP socket to receive multicast packets. NB Receive only!
 *
 * \param[in]   address_string         IPv4 address in ascii e.g. "192.168.1.1"  
 * \param[in]   port                   Port, 0 - 65535 
 * 
 * \return socket or -1 on error
 */
int establish_multicast_socket(struct sockaddr_in *ipv4_address, int port, int type)
{
    int socket = -1;
    //struct hostent *hp;
    //int err;
    //int i;
    //char tempaddrstring[50];


    //memset(&dns_cache_response.addr, 0, sizeof(ip_addr_t));

    //printf("Attempting to create socket for %s port %d\n", address_string, port);

    // for (i=0; i<3; i++)
    // {
    //     watchdog_pulse();

    //     hp = gethostbyname(address_string);  // blocking call

    //     watchdog_pulse();

    //     if (!hp)
    //     {
    //         printf("get host by name returned NULL\n");
    //     }
    //     else
    //     {
    //         printf("Got IP!\n");
    //         break;
    //     }
    //     SLEEP_MS(1000);
    // }

    memset(ipv4_address, 0, sizeof(struct sockaddr_in));
    ipv4_address->sin_len = sizeof(ipv4_address);
    ipv4_address->sin_family = AF_INET;
    ipv4_address->sin_port = PP_HTONS(port);  //<===== check, are we setting the source port or the destination port?


    ipv4_address->sin_addr.s_addr = INADDR_ANY;

    // if(!hp)
    // {
    //     ipv4_address->sin_addr.s_addr = inet_addr(address_string);  // string with numerical IP address only
    // }
    // else
    // {
    //     ipv4_address->sin_addr.s_addr = *((u32_t *)(hp->h_addr)); //string with either hostname or numerical IP address   

    //     //printf("%d.%d.%d.%d\n", ((char *)(ipv4_address->sin_addr.s_addr))[0], ((char *)(ipv4_address->sin_addr.s_addr))[1], ((char *)(ipv4_address->sin_addr.s_addr))[2],((char *)(ipv4_address->sin_addr.s_addr))[3] );
    // }

    socket = lwip_socket(AF_INET, type, 0);

    printf("socket = %d\n", socket);

    if (socket >= 0)
    {
        if (socket > web.socket_max) web.socket_max = socket;
        
        if (lwip_bind(socket, (struct sockaddr *)ipv4_address , sizeof(struct sockaddr_in)))
        {
            printf("closing socket due to bind failure\n");
            lwip_close(socket); 
            socket = -1;
            web.bind_failures++;
        }
        else
        {
           JoinGroup(socket, "239.255.255.250", ipaddr_ntoa(netif_ip4_addr(&cyw43_state.netif[0]))); 
        }
    }

    return(socket);
}

int JoinGroup(int sock, const char* join_ip, const char* local_ip)
{
  ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(join_ip);
  mreq.imr_interface.s_addr = inet_addr(local_ip);
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    return -1;
  return 0;
}



#ifdef USE_GETHOSTBYNAME
/*!
 * \brief Create a TCP or UDP socket
 *
 * \param[in]   address_string         IPv4 address in ascii e.g. "192.168.1.1"  
 * \param[in]   port                   Port, 0 - 65535 
 * 
 * \return socket or -1 on error
 */
int establish_socket(char *address_string, /*struct sockaddr_in *ipv4_address,*/ int port, int type)
{
    int socket = -1;
    struct hostent *hp;
    int err;
    int i;
    char tempaddrstring[50];


    //memset(&dns_cache_response.addr, 0, sizeof(ip_addr_t));

    printf("Attempting to create socket for %s port %d\n", address_string, port);

    for (i=0; i<10; i++)
    {
        //watchdog_pulse();
        //cyw43_arch_lwip_begin();
        hp = gethostbyname(address_string);  // blocking call
        //cyw43_arch_lwip_end();
        //watchdog_pulse();

        if (!hp)
        {
            printf("gethostbyname() returned NULL [looking up %s]\n", address_string);
        }
        else
        {
            //printf("Got IP!\n");
            break;
        }
        SLEEP_MS(1000);
    }

    memset(ipv4_address, 0, sizeof(struct sockaddr_in));
    ipv4_address->sin_len = sizeof(ipv4_address);
    ipv4_address->sin_family = AF_INET;
    ipv4_address->sin_port = PP_HTONS(port);

    if(!hp)
    {
        ipv4_address->sin_addr.s_addr = inet_addr(address_string);  // string with numerical IP address only
    }
    else
    {
        ipv4_address->sin_addr.s_addr = *((u32_t *)(hp->h_addr)); //string with either hostname or numerical IP address   

        //printf("%d.%d.%d.%d\n", ((char *)(ipv4_address->sin_addr.s_addr))[0], ((char *)(ipv4_address->sin_addr.s_addr))[1], ((char *)(ipv4_address->sin_addr.s_addr))[2],((char *)(ipv4_address->sin_addr.s_addr))[3] );
    }

    //cyw43_arch_lwip_begin();
    socket = socket(AF_INET, type, 0);
    //cyw43_arch_lwip_end();

    socket = get_socket(address_string, port, type);

    printf("socket = %d for %s : %d\n", socket, address_string, port);

    if (socket >= 0)
    {
        if (socket > web.socket_max) web.socket_max = socket;

        //cyw43_arch_lwip_begin();        
        if (connect(socket, (struct sockaddr *)ipv4_address, sizeof(struct sockaddr_in)))
        {
            printf("closing socket due to connect failure [%s, %d, %d]\n", address_string, port, type);

            close(socket); 

            socket = -1;
            web.connect_failures++;
        }
        //cyw43_arch_lwip_end();
    }

    return(socket);
}
#else
/*!
 * \brief get_address from IPv4 address or hostname in ascii
 *
 * \param[in]   address_string         IPv4 address or hostname in ascii e.g. "192.168.1.1" or "google.com"   
 * \param[in]   port                   Port, 0 - 65535 
 * \param[in]   type                   STREAM or DATAGRAM 
 * 
 * \return socket or -1 on error
 */
int establish_socket(char *address_string, /*struct sockaddr_in *ipv4_address,*/ int port, int type)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int socket, s;
    //size_t len;
    //ssize_t nread;
    //char buf[BUF_SIZE];
    char port_string[12];

    sprintf(port_string, "%d", port);
    socket = -1;

    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;      /* Allow IPv4 only */
    hints.ai_socktype = type;       /* Stream or Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(address_string, port_string, &hints, &result);
    if (s != 0)
    {
        printf("error returned from getaddrinfo [%s, %s, %d]\n", address_string, port_string, type);
    }
    else
    {
        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
            printf("Trying to open socket with family = %d socktype = %d protocol = %d [%s, %s, %d]\n", rp->ai_family, rp->ai_socktype, rp->ai_protocol, address_string, port_string, type);
            socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socket >= 0)
            {
                //printf("Got socket [%s, %s, %d]\n", address_string, port_string, type);
                if (socket > web.socket_max) web.socket_max = socket;
    
                if (!connect(socket, rp->ai_addr, rp->ai_addrlen))
                {
                    // successfully connected socket
                    printf("socket conntected [%s, %d, %d]\n", address_string, port, type);
                    break;
                } 
                else
                {
                    // failed to connect so try next address returned by DNS
                    printf("connect failed -- closing socket[%s, %d, %d]\n", address_string, port, type);

                    close(socket); 

                    socket = -1;
                    web.connect_failures++;
                }
            }
            else
            {
                printf("socket error = %d\n", socket);
            }
        }

        // delete linked list of dns results
        freeaddrinfo(result);
    }

    return(socket);
}
#endif

