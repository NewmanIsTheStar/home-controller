/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef SYSTEM_WEB_H
#define SYSTEM_WEB_H


typedef struct SYS_WEB_VARIABLES
{
  uint32_t boot_time;
  uint8_t mac[6];
  int access_point_mode;
  char watchdog_timestring[50];
  uint32_t us_last_rx_packet;
  char status_message[100];
  char stack_message[256];
  char ip_address_string[50];
  char network_mask_string[50];
  char gateway_string[50];
  int socket_max;
  int bind_failures;
  int connect_failures;  
  int syslog_transmit_failures;
  int pluto_transmit_failures;
  char software_server[100];     
  char software_url[100];        
  char software_file[100];
  int software_download_now;    
  char edit_text_filename[16]; 
  char basic_file_to_execute[16];        
} SYS_WEB_VARIABLES_T;                  //remember to add initialization code when adding to this structure !!!

int system_web_init(void);

#endif

