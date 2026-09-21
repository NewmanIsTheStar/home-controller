/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 // be cautious what you include here lwip and fcntl have some important conflicts related to BSD / sockets
#include <stdio.h>
#include <stdlib.h>
//#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <lwip/arch.h>
#include "picofs.h"
#include "syscfg.h"
#include "system_config.h"
#include "application_config.h"


#include <sys/stat.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "syscfg.h"
#include "pluto.h"
#include "utility.h"

#include "flash.h"
#include "picofs.h"


#define SYSTEM_CONFIG_FILE "system.cfg"


//SYSTEM_CONFIG_T *sys = NULL;
//SYSTEM_CONFIG_T system_config;



// /*!
//  * \brief Copy configuration from flash to RAM
//  * 
//  * \return 0 on success, -1 on error
//  */
// int syscfg_map_file(void)
// {
//     int err = 0;
//     struct stat file_status;

//     // map file into RAM, create the file if it doesn't already exist
//     err = syscfg_mmap(SYSTEM_CONFIG_FILE);
//     if (err)
//     {
//         printf("syscfg_map_file: failed to mmap file: %s\n", SYSTEM_CONFIG_FILE);
//     }          
    
//     return(err);
// }

/*!
 * \brief Copy configuration from RAM into flash
 * 
 * \return 0 on success
 */
int syscfg_sync_file(void *configuration_buffer, int configuration_len)
{
    int err = 0;

    err = picofs_msync(configuration_buffer, configuration_len, MS_SYNC);

    return(err);
}

/*!
 * \brief get a pointer to the configuration in flash memory (i.e. what was last synced rather than what is currently in the RAM cache)
 *
 * \return pointer to file in flash memory or NULL if not found
 */
void *syscfg_get_flash_location(char *filename)
{
    void *location = NULL;
    FILE_TRAILER_T *config_trailer = NULL;


    if (!picofs_find_file(filename, FS_INVALID_FID, &config_trailer))
    {
        location = (char *)config_trailer + sizeof(FILE_TRAILER_T) - config_trailer->file_size;
    }

    //printf("syscfg_get_flash_location: returning system configuration location = %p\n", location);

    return(location);
}


/*!
 * \brief map configuration file into memory for random access
 *
 * \param filename file containing configuration
 * 
 * \return nothing
 */
int syscfg_mmap(char *filename, void **configuration_buffer, size_t config_size) 
{
    int syscfg_fd = -1;
    size_t FILE_SIZE = 4096; // 4 KB (typically matches 1 memory page)
    char *map;

    // open the file with for read/write (create if it doesn't exist)
    syscfg_fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (syscfg_fd == -1) 
    {
        perror("syscfg_mmap: Error opening/creating file");
        return EXIT_FAILURE;
    }
    //printf("config_mmap: config_fd = %d\n", config_fd);

    // adjust the file length to match the current configuration version 
    if (ftruncate(syscfg_fd, config_size) == -1) 
    {
        perror("syscfg_mmap: Error setting file size");
        close(syscfg_fd);
        syscfg_fd = -1;
        return EXIT_FAILURE;
    }

    // map the file into the process address space
    map = picofs_mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, syscfg_fd, 0);
    if (map == MAP_FAILED) 
    {
        perror("syscfg_mmap: Error mapping the file");
        close(syscfg_fd);
        syscfg_fd = -1;
        return EXIT_FAILURE;
    }
    //printf("syscfg_mmap: @%p\n", map);
    
    // update the configuration pointer
    //sys = (SYSTEM_CONFIG_T *)map;
    *configuration_buffer = map;

    close(syscfg_fd);
    
    return EXIT_SUCCESS;
}