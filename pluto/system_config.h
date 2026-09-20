/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <limits.h>


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
* System Configuration memory structure
* Modification Rule 1 -- copy this structure, append a version number(format "_VERSION_X") and place at the bottom of this file before making changes
* Modification Rule 2 -- the first member must be version and the last memeber must be crc             
* Modification Rule 3 -- do not reorder or resize existing members, add new members immediately above crc
* Modification Rule 4 -- add an upgrade function to convert from the previous version and add this function to the config_info table
* Modification Rule 5 -- the struct size must never reduce as conversions occur within memory allocated for the latest version
*/

// current version of system variables
typedef struct
{   
    int version;                           // ***version must be the first member***
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
    uint16_t crc;                          // ***crc must be the last member***
    
} SYSTEM_CONFIG_T;  //_VERSION_1


// extern for all that #include this header file
extern SYSTEM_CONFIG_T *sys;


// previous non-volatile data stuctures -- these are used when upgrading


#endif