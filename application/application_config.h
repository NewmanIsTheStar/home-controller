/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef APPLICATION_CONFIG_H
#define APPLICATION_CONFIG_H

#include <limits.h>


typedef enum
{
    AUTOMATION_UNDEFINED          =   0,            
    AUTOMATION_ENABLED            =   1,           
    AUTOMATION_DISABLED           =   2,          
    
    NO_AUTOMATION                 =   4294967295     // force enum to be 4 bytes long 
} AUTOMATION_STATE_E;

/*
* Application Configuration memory structure
* Modification Rule 1 -- copy this structure, append a version number(format "_VERSION_X") and place at the bottom of this file before making changes
* Modification Rule 2 -- the first member must be version and the last memeber must be crc             
* Modification Rule 3 -- do not reorder or resize existing members, add new members immediately above crc
* Modification Rule 4 -- add an upgrade function to convert from the previous version and add this function to the config_info table
* Modification Rule 5 -- the struct size must never reduce as conversions occur within memory allocated for the latest version
*/

// current version of application variables
typedef struct
{   
    int version;
    int hc_enable;
    AUTOMATION_STATE_E automation_state[64];   
    char automation_name[64][64];
    uint32_t automation_triggered[64];         
    uint16_t crc;
        
} NON_VOL_VARIABLES_T;


// extern for all that #include this header file
extern NON_VOL_VARIABLES_T *cfg;


// previous non-volatile data stuctures -- these are used when upgrading


#endif
