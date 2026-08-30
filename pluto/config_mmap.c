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


extern NON_VOL_VARIABLES_T config;

int config_mmap_test() 
{
    const char *filepath = "database.bin";
    size_t FILE_SIZE = 4096; // 4 KB (typically matches 1 memory page)

    // 1. Create and open the new file with Read/Write permissions
    // O_CREAT: Create file if it doesn't exist.
    // O_TRUNC: Truncate file to 0 bytes if it already exists.
    int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Error opening/creating file");
        return EXIT_FAILURE;
    }
    printf("mmap_test: fd = %d\n", fd);

    // 2. STRETCH THE FILE: Set the storage space before calling mmap
    // Memory mapping cannot dynamically increase the underlying file size.
    if (ftruncate(fd, FILE_SIZE) == -1) {
        perror("Error setting file size");
        close(fd);
        return EXIT_FAILURE;
    }

    // 3. Map the file into the process address space
    // PROT_READ | PROT_WRITE: We want to read and write to this memory region.
    // MAP_SHARED: Changes made to memory are automatically committed to the disk file.
    char *map = picofs_mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("Error mapping the file");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("mmap_test: @%p\n", map);

    // The file descriptor can technically be closed right after mmap(), 
    // but we will keep it standard and close it at the end.

    // 4. Write data directly to the file using memory pointers
    strcpy(map, "Hello, this is text written via mmap() memory manipulation!");
    printf("Data written to memory map successfully.\n");

    // 5. Optional: Synchronize memory changes back to disk immediately
    // Without msync, the OS manages flushing, but msync forces durability.
    // if (msync(map, FILE_SIZE, MS_SYNC) == -1) {
    //     perror("Could not sync file to disk");
    // }

    printf("mmap_test: calling unmap\n");
    // 6. Clean up: Unmap the memory and close the file descriptor
    if (picofs_munmap(map, FILE_SIZE) == -1) {
        perror("Error unmapping the memory");
    }
    
    printf("mmap_test: calling close\n");
    close(fd);
    return EXIT_SUCCESS;
}


/*!
 * \brief write configuration to file
 *
 * \param message one byte message
 * 
 * \return nothing
 */
int config_write_to_file(char *filename) 
{
    FILE *file = NULL;
    size_t written_struct = 0;

    // open the file in binary write mode 
    file = fopen(filename, "wb");
    if (file == NULL) 
    {
        perror("Error opening file");
        return 1;
    }

    // write config struct to file
    written_struct = fwrite(&config, sizeof(NON_VOL_VARIABLES_T), 1, file);
    if (written_struct != 1) 
    {
        perror("Error writing config");
        fclose(file);
        return 1;
    }

    // close the file stream
    fclose(file);

    printf("Binary data successfully written to %s\n", filename);

    return EXIT_SUCCESS;
}