/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "pico/cyw43_arch.h"
#include "calendar.h"
#include "pluto.h"
#include "system_web.h"
#include "application_web.h"

// global variables
SYS_WEB_VARIABLES_T sys_web;

/*!
 * \brief Initialize web interface variables
 *
 * \return 0 on success
 */
int system_web_init(void)
{
    // zero web structure
    memset(&sys_web, 0, sizeof(sys_web));

    cyw43_hal_get_mac(CYW43_ITF_STA, sys_web.mac);
    printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", sys_web.mac[0], sys_web.mac[1], sys_web.mac[2], sys_web.mac[3], sys_web.mac[4], sys_web.mac[5]);

    sys_web.access_point_mode = 0;

    sys_web.status_message[0] = 0;
    sys_web.stack_message[0] = 0;

    sys_web.socket_max = 0;
    sys_web.bind_failures = 0;
    sys_web.connect_failures = 0;  
    sys_web.syslog_transmit_failures = 0;

    sys_web.bind_failures = 0;
    sys_web.connect_failures = 0;   

    STRNCPY(sys_web.software_server,"psycho.badnet", sizeof(sys_web.software_server));
    STRNCPY(sys_web.software_url,"http://psycho.badnet/hc", sizeof(sys_web.software_url));
    STRNCPY(sys_web.software_file,"/pluto.bin", sizeof(sys_web.software_file));        
    sys_web.software_download_now = 0;

    application_web_init();
    
    return(0);
}