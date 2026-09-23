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
#include "config.h"
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
#include "config.h"
#include "pluto.h"
#include "utility.h"
#include "picofs.h"




/*!
 * \brief Copy configuration from RAM into flash
 * 
 * \return 0 on success
 */
int config_sync_file(void *configuration_buffer, int configuration_len)
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
void *config_get_flash_location(char *filename)
{
    void *location = NULL;
    FILE_TRAILER_T *config_trailer = NULL;


    if (!picofs_find_file(filename, FS_INVALID_FID, &config_trailer))
    {
        location = (char *)config_trailer + sizeof(FILE_TRAILER_T) - config_trailer->file_size;
    }

    //printf("config_get_flash_location: returning system configuration location = %p\n", location);

    return(location);
}


/*!
 * \brief map configuration file into memory for random access
 *
 * \param filename file containing configuration
 * 
 * \return nothing
 */
int config_mmap(char *filename, void **configuration_buffer, size_t config_size) 
{
    int config_fd = -1;
    size_t FILE_SIZE = 4096; // 4 KB (typically matches 1 memory page)
    char *map;

    // open the file with for read/write (create if it doesn't exist)
    config_fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (config_fd == -1) 
    {
        perror("config_mmap: Error opening/creating file");
        return EXIT_FAILURE;
    }
    //printf("config_mmap: config_fd = %d\n", config_fd);

    // adjust the file length to the requested size 
    if (ftruncate(config_fd, config_size) == -1) 
    {
        perror("config_mmap: Error setting file size");
        close(config_fd);
        config_fd = -1;
        return EXIT_FAILURE;
    }

    // map the file so that tasks can access it directly as memory (rather than using file i/o)
    map = picofs_mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, config_fd, 0);
    if (map == MAP_FAILED) 
    {
        perror("config_mmap: Error mapping the file");
        close(config_fd);
        config_fd = -1;
        return EXIT_FAILURE;
    }
    
    //printf("config_mmap: @%p\n", map);
    
    // update the configuration pointer
    *configuration_buffer = map;

    close(config_fd);
    config_fd = -1;

    return EXIT_SUCCESS;
}