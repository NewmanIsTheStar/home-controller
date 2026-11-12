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
 * \brief connect to wifi
 *
 * \param none
 *
 * \return nothing
 */
int pluto_wifi_connect(void)
{
    ip_addr_t ip = {0};
    ip_addr_t nm = {0};
    ip_addr_t gw = {0};

#ifdef TEST_AP_MODE 
    // use this mode for development when a wifi network is not available
    pluto_ap_mode_for_developers();
    retrun(0);
#endif   

    // enable wifi station mode
    cyw43_arch_enable_sta_mode();

    // set hostname
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(n, "usurper");              // TODO: change to name from config

    //connect to wifi network
    printf("Connecting to Wi-Fi...\n");
    if (!cyw43_arch_wifi_connect_timeout_ms(sys->wifi_ssid, sys->wifi_password, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Connected.\n");
    } 
    else
    {
        printf("failed to connect to network.\n");

        // enable wifi access point mode -- so user can perform initial configuration
        pluto_ap_mode();
    }

    // handle static ip config
    if (!sys->dhcp_enable)
    {
        printf("Using static IP settings\n");
        inet_pton(AF_INET, sys->ip_address, &ip);
        inet_pton(AF_INET, sys->network_mask, &nm);
        inet_pton(AF_INET, sys->gateway, &gw);
        netif_set_addr(netif_default, &(ip), &(nm), &(gw));
    }

    return(0);
}