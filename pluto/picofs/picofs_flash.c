/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <fcntl.h>
#include "pico/flash.h"
#include <hardware/flash.h>
#include "picofs.h"




// typedefs
typedef struct picofs_flash_range
{
    uint32_t dst_offset;
    char *src;  
    size_t len;
} PICOFS_FLASH_RANGE_T;




//prototypes


// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables



/*!
 * \brief Shim for programming flash pages with second core suspended and interrupts disabled
 * 
 * \param[in]   ptr flash offset, RAM source and data length 
 */
void __no_inline_not_in_flash_func(picofs_flash_program_shim)(void *ptr)
{
    PICOFS_FLASH_RANGE_T *parameters = ptr;

    if (ptr)
    {
        if (parameters->dst_offset%FS_PAGE_SIZE)
        {
            return;
        }

        if ((parameters->len%FS_PAGE_SIZE) || ((char *)(parameters->len + parameters->dst_offset + FS_FLASH_BASE) > FS_END))  
        {
            return;
        }

        // program the configuation in 256 byte pages (range is rounded up to the nearest multiple of 256 Bytes)
        #if FS_FAKE_FLASH
            memcpy((char *)(FS_FLASH_BASE+parameters->dst_offset), parameters->src, parameters->len);
        #else            
            flash_range_program(parameters->dst_offset, parameters->src, parameters->len);
        #endif
    }

    return;
}

/*!
 * \brief program flash pages (256 bytes)
 * 
 * \return 0 on success
 */
int picofs_flash_program(char *dst, char *src, size_t len)
{
    int err = 0;
    uint32_t flash_offset = (uint32_t)dst - (uint32_t)FS_FLASH_BASE;
    PICOFS_FLASH_RANGE_T shim_parameters;

    if (flash_offset%FS_PAGE_SIZE)
    {
        picofs_printf("picofs: error: picofs_flash_program(): invalid page offset (%d)\n", flash_offset);
        return(-1);
    }

    if ((len%FS_PAGE_SIZE) || ((dst+len) > FS_END))
    {
        picofs_printf("picofs: error: picofs_flash_program(): invalid length (%#0x) [dst+len = %#0p FS_END = $%p]\n", len, dst+len, FS_END);
        return(-2);
    }

    // prepare parameters for shim
    shim_parameters.dst_offset = flash_offset;
    shim_parameters.src = src;
    shim_parameters.len = len;

    //picofs_printf("FLASH PROGRAM: offset = %#0x len = %#0x plus FS_BASE = %#0x\n", shim_parameters.dst_offset, shim_parameters.len, FS_FLASH_BASE+shim_parameters.dst_offset+shim_parameters.len);
    //hex_dump(src, len);

    err = flash_safe_execute(picofs_flash_program_shim, &shim_parameters, 5000);

    if (err)
    {
        picofs_printf("picofs: error programming flash: flash_safe_execute() returned error %d\n", err);

        #ifdef BREAKPOINT_FLASH_WRTIE_FAIL        
            // Hardcoded breakpoint instruction tells the SWD debugger to freeze right here
            __asm volatile("bkpt #0"); 
            while(1); // Infinite loop prevents the chip from executing further code/resetting
        #endif
    }
    else
    {
        #if FLASH_CACHE_DUPLICATION_POSSIBLE == 1
            flash_flush_cache();
        #endif

        // verify programming result
        err = memcmp(dst, src, len);

        if (err)
        {
            picofs_printf("picofs: error: picofs_flash_program(): verification failed @%p\n", dst);
        }
    }
    
    return(err);
}




/*!
 * \brief Shim for erasing flash pages with second core suspended and interrupts disabled
 * 
 * \param[in]   ptr flash offset, RAM source and data length 
 */
void __no_inline_not_in_flash_func(picofs_flash_erase_shim)(void *ptr)
{
    PICOFS_FLASH_RANGE_T *parameters = ptr;

    if (ptr)
    {
        if (parameters->dst_offset%FS_PAGE_SIZE)
        {
            return;
        }

        if ((parameters->len%FS_PAGE_SIZE) || ((char *)(parameters->len + parameters->dst_offset + FS_FLASH_BASE) > FS_END))
        {
            return;
        }

        // erase a flash block (4 KBytes)
        #if FS_FAKE_FLASH
            memset((char *)(FS_FLASH_BASE+parameters->dst_offset), FS_ERASED_CELL_VALUE, parameters->len);
        #else            
            flash_range_erase(parameters->dst_offset, parameters->len);
        #endif   

    }

    return;
}

/*!
 * \brief erase flash sectors (4096 bytes)
 * 
 * \return 0 on success
 */
int picofs_flash_erase(char *dst, size_t len)
{
    int err = 0;
    uint32_t flash_offset = (uint32_t)dst - (uint32_t)FS_FLASH_BASE;
    PICOFS_FLASH_RANGE_T shim_parameters;    

    if (flash_offset%FS_SECTOR_SIZE)
    {
        picofs_printf("picofs: error: picofs_flash_erase(): invalid sector offset\n");
        return(-1);
    }

    if ((len%FS_SECTOR_SIZE) || ((dst+len) > FS_END))        
    {
        // BUG!  picofs: error: picofs_flash_erase(): invalid length (1024) [offset = 2048 fs_size = 2560

        picofs_printf("picofs: error: picofs_flash_erase(): invalid length ($%0x) [dst+len= $%p FS_END = $%p]\n", dst+len, FS_END);
        return(-2);
    }
   
    // prepare parameters for shim
    shim_parameters.dst_offset = flash_offset;
    shim_parameters.src = NULL;
    shim_parameters.len = len;

   
    err = flash_safe_execute(picofs_flash_erase_shim, &shim_parameters, 5000);

    if (err)
    {
        picofs_printf("picofs: error erasing flash: flash_safe_execute() returned error %d\n", err); 
    } 
    else
    {
        #if FS_BASE == _u(0x10400000) 
        flash_flush_cache();
        #endif
    }  

    return(err);
}

/*!
 * \brief erase a sequential set of sectors
 * 
 * \param[in]   start_sector     first sector to erase
 * 
 * \param[out]  end_sector       last sector to erase
 *  *     
 * \return 0 on success
 */
int picofs_flash_erase_sector_range(int start_sector, int end_sector)
{
    int err = 0;
    
    picofs_flash_erase(FS_START + start_sector*FS_SECTOR_SIZE, (end_sector-start_sector+1)*FS_SECTOR_SIZE);

    return(err);
}
