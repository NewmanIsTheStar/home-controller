
/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PLUTO_H
#define PLUTO_H

#ifndef USEFUL_MACROS
    #define USEFUL_MACROS
    #define STRNCPY(dst, src, size)  {strncpy((dst), (src), (size)); *((dst)+(size)-1)=0;}
    #define STRNCAT(dst, src, size)  {if ((size) > 0) {*((dst)+(size)-1)=0; strncat((dst), (src), (size)-strlen((dst))-1);};}
    #define STRAPPEND(dst, src)  {*((dst)+(sizeof(dst))-1)=0; strncat((dst), (src), (sizeof(dst))-strlen((dst))-1);}
    #define NUM_ROWS(x) (sizeof(x)/sizeof(x[0]))
    #define MASKED_WRITE(dest,src,mask) {(dest) = (((dest) & (~(mask))) | ((src) & (mask)));}
    #define CLIP(x, low, high)  (x=(((x)>(high))?(high):(((x)<(low))?(low):(x))))
    #define SLEEP_MS(x) (vTaskDelay(x));
    #define FORCE_STRING_TERMINATION(x) {x[sizeof(x)-1]=0;}
    #define SAFEAROUND(x) ((x) >= 0 ? (int)((x) + 0.5f) : (int)((x) - 0.5f))
#endif

typedef enum
{
    REBOOT_USER_REQUEST     = 0,
    REBOOT_SNTP_FAILURE     = 1,
    REBOOT_WEATHER_FAILURE  = 2,
    REBOOT_WATCHDOG         = 3,
    REBOOT_SOFTWARE_UPGRADE = 4, 
    REBOOT_MQTT_F2          = 5,   
    REBOOT_MQTT_F3          = 6,   
    REBOOT_MQTT_F4          = 7,   
    REBOOT_MQTT_F5          = 8,                      
    REBOOT_UNKNOWN          = 4294967295,   //INT_MAX inadequate 
} REBOOT_REASON_T;

typedef struct 
{
    uint32_t magic_start0;
    uint32_t magic_start1;
    uint32_t flags;
    uint32_t target_addr;
    uint32_t payload_size;
    uint32_t block_no;
    uint32_t num_blocks;
    union 
    {
        uint32_t file_size;
        uint32_t family_id;
    };
    uint8_t  data[476];
    uint32_t magic_end;
} uf2_block_t;

//int applet_entry_point(void);
int pluto_application_restart(REBOOT_REASON_T reason);
uint32_t pluto_get_reboot_reason(void);
int pluto_boot_setup(uint32_t exe_flash_offset, char *exe_linker_start_addr);
int pluto_boot_launch(void);
void pluto_boot_menu_task(void *pvParameters);
int pluto_boot_status(void);
void pluto_write_uf2_block(int fd, const uf2_block_t *block);

#endif 
