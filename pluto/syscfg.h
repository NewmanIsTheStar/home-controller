/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include <limits.h>

int syscfg_read(void);
int syscfg_write(void);
void syscfg_changed(void);
bool syscfg_dirty(bool clear_flag);
int syscfg_map_file(void);
int syscfg_mmap(char *filename);
int syscfg_sync_file(void);
void *syscfg_get_flash_location(void);
bool syscfg_compare_flash_ram(bool stop_at_first_difference, bool print_differences);
int syscfg_validate(void);

// device personality
typedef enum
{
    SPRINKLER_USURPER          =   0,             // add wifi control to exising "dumb" sprinkler controller
    SPRINKLER_CONTROLLER       =   1,             // multizone sprinkler control 
    LED_STRIP_CONTROLLER       =   2,             // allows remote control of an led strip
    HVAC_THERMOSTAT            =   3,             // wifi confrolled thermostat
    HOME_CONTROLLER            =   4,             // home controller
    REMOTE_SWITCH              =   5,             // wifi controlled relays
    
    NO_PERSONALITY             =   4294967295     // force enum to be 4 bytes long 
} PERSONALITY_E;


// non-vol structure conversion info
typedef struct
{
    int version;
    size_t version_offset;
    size_t crc_offset;
    void (*upgrade_function)(void *previous_config);
} SYSTEM_CONVERSION_T;

// gpio defaults
typedef enum
{
    GP_UNINITIALIZED          =   0,         
    GP_INPUT_FLOATING         =   1,              
    GP_INPUT_PULLED_HIGH      =   2,             
    GP_INPUT_PULLED_LOW       =   3,
    GP_OUTPUT_HIGH            =   4,
    GP_OUTPUT_LOW             =   5,
    
    GP_LAST                   =   4294967295     // force enum to be 4 bytes long 
} GPIO_DEFAULT_T;

/*
* current system variable memory structure
* Modification Rule 1 -- copy this structure, append a version number(format "_VERSION_X") and place at the bottom of this file before making changes
* Modification Rule 2 -- only add new fields, do not reorder or resize existing fields (except crc)
* Modification Rule 3 -- crc field must always be last (used to find end of config in flash)
* Modification Rule 4 -- add an upgrade function to convert from previous version and add this function to the config_info table
* Modification Rule 5 -- the struct size must never reduce as conversions occur within memory allocated for the latest version
*/

// current version of system variables
typedef struct
{   
    // ***system config start ***
    int version;
    PERSONALITY_E personality;
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_country[32];
    char dhcp_enable;
    char host_name[32];
    char ip_address[32];
    char network_mask[32];    
    char gateway[32];      
    int timezone_offset;
    char daylightsaving_enable;
    char daylightsaving_start[32];
    char daylightsaving_end[32];
    char time_server[4][32];
    int syslog_enable;
    char syslog_server_ip[32];    
    int use_archaic_units; 
    int use_simplified_english;
    int use_monday_as_week_start; 
    GPIO_DEFAULT_T gpio_default[29];
    char mqtt_user[32];
    char mqtt_password[32];
    char mqtt_broker_address[32];
    double latitude;
    double longitude;     
    // ***system config end*** 
    uint16_t crc;
    
} SYSTEM_CONFIG_T;  //_VERSION_1

// extern for all that #include this header file
extern SYSTEM_CONFIG_T *sys;


// previous non-volatile data stuctures -- used when upgrading


#endif