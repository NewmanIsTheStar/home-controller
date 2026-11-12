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


// external variables
extern uint32_t unix_time;
extern APP_CONFIG_T config;
extern SYS_WEB_VARIABLES_T sys_web;
extern WORKER_TASK_T worker_tasks[];


// static variable
static long long atomic_variable = 0;



int test_myfilesystem(void)
{
    FILE *filePointer;
    char buffer[256];

    // 1. Open the file in read mode ("r")
    filePointer = fopen("monkey", "r");

    // 2. Check if the file exists and opened successfully
    if (filePointer == NULL) {
        printf("Error: Could not open file.\n");
        return 1; 
    }

    // 3. Read and print the file line-by-line
    while (fgets(buffer, sizeof(buffer), filePointer) != NULL)
    {
        printf("%s", buffer);
    }

    // 4. Close the file to free up system resources
    fclose(filePointer);

    return 0;
}


int test_stat(void) 
{
    // 1. Define the path to your file
    const char *filename = "goat";
    
    // 2. Create a stat structure to hold the file details
    struct stat file_status;

    // 3. Call stat() passing the filename and the address of your structure
    if (stat(filename, &file_status) != 0) {
        // If stat returns anything other than 0, an error occurred
        fprintf(stderr, "Error reading file status: %s\n", strerror(errno));
        return 1;
    }

    // 4. Extract the file size from the st_size member variable
    // st_size is natively represented in bytes
    long long file_size = (long long)file_status.st_size;

    printf("File: %s\n", filename);
    printf("Size: %lld bytes\n", file_size);

    return 0;
}


void atomic_write_task(__unused void *params)
{
    int i = 0;
    int j = 0;
    int write_core = 0;

    write_core = portGET_CORE_ID();
    
    printf("Atomic Write Test starting on core %d!\n", write_core);
    
    for(;;)
    {
        if (i)
        {
            atomic_variable = 0xAAAAAAAAAAAAAAAA;
        }
        else
        {
            atomic_variable = 0xCCCCCCCCCCCCCCCC;
        }

        i = !i;

        if (++j>=1000000000)
        {
            if (write_core != portGET_CORE_ID())
            {
                printf("WRITE CORE CHANGED!\n");
                write_core = portGET_CORE_ID();
            }
            printf("*MARK*  One billion writes\n");
            atomic_variable = 0;
            j = 0;
        }        
    }

} 

void atomic_read_task(__unused void *params)
{
    long long i = 0;
    int j = 0;
    int read_core = 0;

    read_core = portGET_CORE_ID();    

    printf("Atomic Read Test starting on core %d!\n", read_core);

    for(;;)
    {
        i = atomic_variable;

        if ((i != 0xAAAAAAAAAAAAAAAA) && (i != 0xCCCCCCCCCCCCCCCC))
        {
            printf("%llx\n", i);
            sleep_ms(1000);
        }

        if (++j>=1000000000)
        {
            if (read_core != portGET_CORE_ID())
            {
                printf("READ CORE CHANGED!\n");
                read_core = portGET_CORE_ID();
            }
            printf("*MARK*  One billion reads\n");
            j = 0;
        }
    }

} 


    //  TEST TEST TEST see pluto_terminal.c
    // xTaskCreate(vMenuInterfaceTask, "Boot Menu", 1024, NULL, 1, &task);  
    // vTaskStartScheduler();
    // printf("PAUSE\n");
    // sleep_ms(5000);