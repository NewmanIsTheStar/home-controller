/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "lwip/netdb.h"
#include "FreeRTOS.h"
#include "picofs.h"

//prototypes
int picofs_get_fd_from_mmap_address(void *addr) ;

// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables


/*!
 * \brief crude mmap
 * 
 * \param addr     ignored -- set to NULL
 * \param len      number of bytes to map
 * \param prot     memory protection
 * \param flags    mapping attributes
 * \param fd       open file descriptor
 * \param offset   offset to start of mapping e.g. offset of zero maps from the start of file
 * \return pointer to mapping on success
 */
void *picofs_mmap(void *addr, size_t len, int prot, int flags, int fd, uint32_t offset) 
{
    int target_fd = fd - 3;

    // handle RAM-only anonymous mappings
    if (flags & MAP_ANONYMOUS) 
    {
        if (!(prot & (PROT_READ | PROT_WRITE))) 
        {
            errno = EINVAL;
            return(MAP_FAILED);
        }
        
        void *ptr = malloc(len);
        
        if (!ptr) 
        {
            errno = ENOMEM;
            return(MAP_FAILED);
        }
        
        return ptr;
    }

    // handle file mappings
    if (flags & MAP_SHARED)
    {
        if ((fd < 3) || (target_fd >= FS_MAX_FILE_DESCRIPTORS) || !custom_fds[target_fd].in_use)
        {
            errno = EBADF;
            return(MAP_FAILED);
        }

        if (prot & PROT_WRITE) 
        {
            if (offset > (custom_fds[target_fd].data_len))
            {
                return(MAP_FAILED);
            }

            custom_fds[target_fd].mmap_ref_count++;

            return((void *)(custom_fds[target_fd].cache + offset));
        }
        else if (prot & PROT_READ)
        {
            if (offset > (custom_fds[target_fd].data_len))
            {
                return(MAP_FAILED);
            }

            custom_fds[target_fd].mmap_ref_count++;

            return((void *)(custom_fds[target_fd].file + offset));
        }        
    }

    // handle read-only direct flash mappings
    if (prot & PROT_WRITE) 
    {
        // microcontroller flash cannot be written directly via pointer like RAM
        errno = EACCES; 
        return(MAP_FAILED);
    }

    // calculate direct pointer in flash file system address space
    uintptr_t picofs_raw_address = FS_BASE + offset;
    
    // ensure bounds match standard flash boundaries
    if (offset + len > PICO_FLASH_SIZE_BYTES) 
    {
        errno = ENXIO;
        return(MAP_FAILED);
    }

    return ((void *)picofs_raw_address);
}

/**
 * Unmaps the memory allocated or pointed to by pico_mmap
 */
int picofs_munmap(void *addr, size_t len) 
{
    uintptr_t address = (uintptr_t)addr;
    int fd = -1;

     
    fd = picofs_get_fd_from_mmap_address(addr);

    if ((fd >= 0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {

        if (custom_fds[fd].mmap_ref_count > 0)
        {
            custom_fds[fd].mmap_ref_count--;   // TODO: needs to be atomic
        }

        // release fd if the file was closed but fd was held open by this mapping
        if ((custom_fds[fd].mmap_delayed_close) && (custom_fds[fd].mmap_ref_count == 0))
        {
            close(fd);
        }
        
        return(0);
    }
    else
    {
        // if it points inside the Flash XIP window, nothing to free
        if (address >= FS_BASE && address < (FS_BASE + PICO_FLASH_SIZE_BYTES)) 
        {
            // do nothing
        }

        // RAM not associated with a file descriptor so assume anonymous mapping and free the malloc'd buffer
        if (addr != NULL && addr != MAP_FAILED) 
        {
            free(addr);
            return(0);
        }
    }

    //errno = EINVAL;
    return(-1);
}


int picofs_get_fd_from_mmap_address(void *addr) 
{
    int i;
    int fd = -1;

    for(i=0; i < FS_MAX_FILE_DESCRIPTORS; i++)
    {
        // check if matches a file cache 
        if ((addr >= (void *)custom_fds[i].cache) && (addr < (void *)(custom_fds[i].cache + custom_fds[i].cache_len)))
        {
            fd = i;
            break;
        }

        // check if matches a file (i.e. mmap readonly which points directly to a file in flash)
        if ((addr >= (void *)custom_fds[i].data) && (addr < (void *)(custom_fds[i].data + custom_fds[i].data_len)))
        {
            fd = i;
            break;
        }        
    }
    
    return(fd);
}

int picofs_msync(void *addr, size_t length, int flags)
{
    int err = -1;
    int fd = -1;
    int fid = FS_INVALID_FID;
    
    fd = picofs_get_fd_from_mmap_address(addr);

    if ((fd >= 0) && (fd < FS_MAX_FILE_DESCRIPTORS))
    {
        if (!picofs_sync_file_regular(fd, false))
        {
            err = 0;

            // remember fid in case of rollover
            fid = custom_fds[fd].cache_trailer.file_id;

            // increment sequence number 
            picofs_increment_sequence(&(custom_fds[fd].cache_trailer));

            if (custom_fds[fd].cache_trailer.file_id != fid)
            {
                // rollover occured to a new fid so schedule the deletion of the file with old fid
                // the deletion occurs during the next msync or close of the file 
                custom_fds[fd].rollover_fid = fid;  
            }
        }
    }

    return(err);
}
