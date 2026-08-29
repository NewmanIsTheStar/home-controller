/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>

void config_changed(void);
bool config_dirty(bool clear_flag);
int config_timeserver_failsafe(void);
int config_read(void);
int config_write(void);
int config_mmap_test();

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
} NON_VOL_CONVERSION_T;

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
* current non-volatile memory structure
* Modification Rule 1 -- copy this structure, append a version number(format "_VERSION_X") and place at the bottom of this file before making changes
* Modification Rule 2 -- only add new fields, do not reorder or resize existing fields (except crc)
* Modification Rule 3 -- crc field must always be last (used to find end of config in flash)
* Modification Rule 4 -- add an upgrade function to convert from previous version and add this function to the config_info table
* Modification Rule 5 -- the config size must never reduce as conversions occur within memory allocated for the latest version
*/

// current version
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
    // ***system config end*** 
    uint16_t system_crc;

    // ***application config start***
    int hc_enable;
    uint8_t shelly_device_ip[64][4];
    uint8_t shelly_device_type[64];
    uint8_t shelly_parameter_device_index[128];
    uint8_t shelly_parameter_name_index[128];
    char shelly_parameter_value[16][32];   //shelly_parameter_value[128][32];
    char shelly_parameter_name[16][32];  // shelly_parameter_name[64][32];    
    char automation_name[32][32];
    uint32_t automation_triggered[32];
    char automation_state[32];
    // ***application config end***    
    uint16_t crc;
    
} NON_VOL_VARIABLES_T;

extern NON_VOL_VARIABLES_T *cfg;
// previous non-volatile data stuctures -- used when upgrading


#endif