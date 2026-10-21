/**
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include <fcntl.h>
#include "pico/cyw43_arch.h"
#include "lwip/apps/fs.h"
#include "lwip/sockets.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"
#include "shell.h"
#include "ping_core.h"
extern MessageBufferHandle_t xMessageBuffer;

// prototypes
int shell_ping(char *ipv4_string);

int shell_queue_ping(char *address)
{
    SM_PING_T msg;

    if (address)
    {
        msg.message_id = SHELL_MSG_PING;
        STRNCPY(msg.address, address, sizeof(msg.address));
        xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);
    }

    return(0);
}

/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_ping(SM_PING_T *msg)
{
    int err = -1;

    if (!msg || !(msg->address[0]))
    {
        // error no filename
        shell_printf("ping: error: no address provided\n");
        return(-1);
    }

    shell_ping(msg->address);

    return(err);
}

/*!
 * \brief ping - icmp echo request
 * \param[in]  ipv4_string  ip address to send icmp echo 
 * \return 0 on success, -1 on error
 */
int shell_ping(char *ipv4_string)
{
    u32_t ip;
    int values[4] = {0,0,0,0};
    u8_t byte = 0;
    int i;

    // static int number_of_shelly_devices = 0;
    // char device_type[32];
    // char device_id[32];
    ip_addr_t ping_addr;
    int ping_err = -1;

    sscanf(ipv4_string, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);
    
    ip   = 0x00000000;
    for(i=0; i<4; i++)
    {
        byte = (u8_t)values[i];
        ip = ip<<8 | byte;
    }
    ping_addr.addr = htonl(ip);

    shell_printf("PING %d.%d.%d.%d with 32 bytes of data\n", ((u8_t *)&ip)[3], ((u8_t *)&ip)[2], ((u8_t *)&ip)[1], ((u8_t *)&ip)[0]);

    ping_err = ping_device(&ping_addr, 3);

    if (!ping_err)
    {
        shell_printf("ping successful\n");
    } 
    else
    {
        shell_printf("ping failed with error %d\n", ping_err);
    }

    return(0);
}
