/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include "lwip/opt.h"
#if !LWIP_HTTPD_SUPPORT_WEBSOCKET
#error "LWIP_HTTPD_SUPPORT_WEBSOCKET must be enabled in lwipopts.h to use this file!"
#endif

#include "hardware/structs/qmi.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include <stdio.h>
#include <sys/stat.h> 
#include <string.h>
#include <errno.h>
#include "hardware/structs/xip_ctrl.h"
#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"
#include "hardware/structs/powman.h"
#include "hardware/regs/powman.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "hardware/sync.h"
#include "hardware/structs/xip_ctrl.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/regs/qmi.h"
#include "hardware/structs/qmi.h"
#include "pico/bootrom.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "lwip/sockets.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "cgi.h"

#include "utility.h"
#include "config.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"
#include "watchdog.h"
#include "worker_tasks.h"
#include "pluto_wifi.h"
#include "calendar.h"
#include "pluto.h"
#include "shell.h"
#include "picofs.h"
#include "config.h"
#include "picofs.h"

#include "ssi.h"
#ifdef USE_GIT_HASH_AS_VERSION
#include "githash.h"
#endif


// Strict hardware base mappings for dependency-free processing
#define RAW_FLASH_BASE      0x1C000000    // RP2350 Physical Flash Flat Array View
#define FLASH_CHIP_SIZE     (4*1024*1024)  // 4 MB Chip Limit
#define SCAN_GRID_STEP      (64*1024)     // 64 KB Grid Alignments

#define QMI_BASE            0x400D0000
#define QMI_ATRANS0         (*(volatile uint32_t *)(QMI_BASE + 0x00))

//#define PPB_BASE            0xE000E000
#define SYSTICK_CTRL        (*(volatile uint32_t *)(PPB_BASE + 0x10))
#define SCB_VTOR            (*(volatile uint32_t *)(PPB_BASE + 0xD08))

// typedef unsigned char  u8_t;
// typedef unsigned int   u32_t;

// typedef struct file_trailer
// {
//     u8_t magic_number[4];   // "pfs"
//     u8_t picofs_version;
//     u8_t file_id;     
//     u8_t file_sequence;
//     u8_t file_status;
//     u32_t file_size;        
//     u32_t crc;
//     char name[16];
// } FILE_TRAILER_T;


// uninitialized variable


// external variables
extern u32_t unix_time;
extern APP_CONFIG_T config;
extern WEB_VARIABLES_T web;
extern WORKER_TASK_T worker_tasks[];

// static variables


// prototypes
void __no_inline_not_in_flash_func(remap_and_boot_app)(uint32_t physical_flash_offset);
u32_t __attribute__((section(".boot_entry"))) picofs_software_crc32(u8_t *ptr, u32_t length);
int pluto_boot_strcmp(char *str_a, char *str_b);
int pluto_find_boot_image(uint32_t *physical_flash_offset);

/*!
 * \brief select and execute code
 *
 * \param none
 *
 * \return 0
 */
int pluto_boot_status(void)
{
    // must be called after stdio initialized
    if ((watchdog_hw->scratch[2] == 0xFEEDC0DE) &&          
        (watchdog_hw->scratch[3] == 0xFEEDC0DE))
    {
        printf("Launching executable with address translation\nNominal Address : %0x \nVirtual Address : %0x\n", XIP_BASE+watchdog_hw->scratch[0], watchdog_hw->scratch[1]);
    }
    else if ((watchdog_hw->scratch[2] == 0xDEADD00D) &&          
             (watchdog_hw->scratch[3] == 0xDEADD00D))
    {
        printf("Launched executable with address translation\nNominal Address : %0x \nVirtual Address : %0x\n", XIP_BASE+watchdog_hw->scratch[0], watchdog_hw->scratch[1]);
    }
    else
    {
        printf("Launched without address translation\n");
    }
}

/*!
 * \brief   load the executable location into watchdog scratch registers in preparation for the next reset
 * \details Address Translation is altered to make it appear that the the code is executing at the expected
 *          location.  The code stored in the file system can then be executed without modification.  
 *          
 *          This allows N copies of the application to be stored in the file system or N different
 *          applications to be stored at once.
 * \param exe_flash_offset       real location of the executable as an offset from the start of flash
 * \param exe_linker_start_addr  location the executable was intended to run from when built
 *
 * \return 0 if scheduler stops (should never happen)
 */
int pluto_boot_setup(u_int32_t exe_flash_offset, char *exe_linker_start_addr)
{
    // addresses
    watchdog_hw->scratch[0] = (u32_t)exe_flash_offset;
    watchdog_hw->scratch[1] = (u32_t)exe_linker_start_addr;    
    
    // inverted addresses 
    watchdog_hw->scratch[2] = ~(u32_t)exe_flash_offset;
    watchdog_hw->scratch[3] = ~(u32_t)exe_linker_start_addr;     

    return(0);
}

/*!
 * \brief select and execute code
 *
 * \param none
 *
 * \return 0
 */
int pluto_boot_launch(void)
{
    uint32_t flash_offset = 0; 

    // check watchdog scratch registers for valid address mapping    
    if ((watchdog_hw->scratch[0] == ~ watchdog_hw->scratch[2]) &&   // data integrity check
        (watchdog_hw->scratch[1] == ~ watchdog_hw->scratch[3]) &&   // data integrity check
        (watchdog_hw->scratch[0] !=   watchdog_hw->scratch[1]) &&   // addresses different  
        (watchdog_hw->scratch[0]%(64*1024) == 0))                   // physical address on 64K boundary
    {
        flash_offset = watchdog_hw->scratch[0];

        // set scratch registers to launch pattern
        watchdog_hw->scratch[2] = 0xFEEDC0DE;
        watchdog_hw->scratch[3] = 0xFEEDC0DE;              

        remap_and_boot_app(flash_offset);
    }
    else if ((watchdog_hw->scratch[2] == 0xFEEDC0DE) &&          
             (watchdog_hw->scratch[3] == 0xFEEDC0DE))
    {        
        // set scratch registers to application running pattern
        watchdog_hw->scratch[2] = 0xDEADD00D;
        watchdog_hw->scratch[3] = 0xDEADD00D;          
    }
    else
    {
        // set scratch registers to bootloader running pattern
        watchdog_hw->scratch[0] = 0x1BADB002;
        watchdog_hw->scratch[1] = 0x1BADB002;
        watchdog_hw->scratch[2] = 0x1BADB002;
        watchdog_hw->scratch[3] = 0x1BADB002; 
        
        rom_flash_reset_address_trans();

        if(!pluto_find_boot_image(&flash_offset))
        {
            pluto_boot_setup(flash_offset, (char *)XIP_BASE);

            // set scratch registers to launch pattern
            watchdog_hw->scratch[2] = 0xFEEDC0DE;
            watchdog_hw->scratch[3] = 0xFEEDC0DE; 

            remap_and_boot_app(flash_offset);

        }
    }
   

    return(0);
}



void __no_inline_not_in_flash_func(remap_and_boot_app)(uint32_t physical_flash_offset) 
{
    // permanently silence interrupts while we manipulate hardware mappings
    uint32_t ints = save_and_disable_interrupts();

    // disable the Watchdog Timer to prevent background chip resets
    watchdog_disable(); 
    
    // completely disable the ARM SysTick Timer and its interrupts
    volatile uint32_t *systick_ctrl = (volatile uint32_t *)0xE000E010;
    *systick_ctrl = 0; 

    // clear out the primary XIP cache to discard obsolete vector mappings
    flash_flush_cache();

    /* 
     * Force an explicit 4MB size pool configuration mask (size = 4) 
     * instead of trying to read back unreadable register bits.
     */
    uint32_t full_chip_size_mask = (4 << 22);
    
    // inject the mapping page along with the explicit 4MB size property
    qmi_hw->atrans[0] = full_chip_size_mask | (physical_flash_offset >> 12); 

    flash_flush_cache();

    // read the vectors directly from the newly translated virtual window
    uint32_t *vector_table = (uint32_t *)XIP_BASE; 
    uint32_t stack_pointer = vector_table[0]; 
    uint32_t reset_handler = vector_table[1]; 

    // enforce ARM Thumb Mode bit on the entry address to avoid a UsageFault
    reset_handler |= 1;

    volatile uint32_t *vtor_reg = (volatile uint32_t *)0xE000ED08;
    *vtor_reg = XIP_BASE;

    // reset the Main Stack Pointer to the new application's stack frame
    __asm volatile ("msr msp, %0" : : "r" (stack_pointer) : "memory");

    // clear PRIMASK to leave the CPU in a clean, raw execution state
    __asm volatile ("cpsie i" : : : "memory");

    // jump directly into the remap binary reset entry vector
    void (*target_entry)(void) = (void (*)(void))reset_handler;
    target_entry();
}



// *******************************************************************************************************


// *******************************************************************************************************

/*!
 * \brief check if filenames are the same
 * \param[in]   str_a     file name to compare
 * \param[out]  str_b     file name to compare
 * \return 0 on success
 */
int pluto_boot_strcmp(char *str_a, char *str_b)
{
    int err = 0;
    int i;

    for(i=0; i<16; i++)
    {
        if (str_a[i] != str_b[i])
        {
            err++;
            break;
        }

        if (str_a[i] == 0)
        {
            break;
        }

        if (str_b[i] == 0)
        {
            break;
        }        
    }

    return(err);
}

/*!
 * \brief find the highest sequence file with the given name
 * \param[in]   filename     name to find
 * \param[out]  trailer      pointer to pointer to file trailer
 * \return 0 on success
 */
int pluto_boot_find_file(char *filename, FILE_TRAILER_T **trailer)
{
    int err = -1;
    int i;
    char * p;
    FILE_TRAILER_T *t = NULL;
    u8_t best_sequence = 0;
    uint32_t calculated_crc = 0;

    // find file called "boot"
    for(p=FS_START+sizeof(FILE_TRAILER_T); p < FS_END; p++)
    {
        if ((p[0] == 'p') && (p[1] == 'f') && (p[2] == 's') && (p[3] == 0))
        {
            t = (FILE_TRAILER_T *)p;

            if ((t->picofs_version == FS_VERION) &&
                !(t->file_status & STS_DELETED) &&
                (!pluto_boot_strcmp(filename, t->name)) &&
                (t->file_sequence >= best_sequence))
            {
                // calculate crc
                calculated_crc = picofs_software_crc32(p + sizeof(FILE_TRAILER_T) - t->file_size, t->file_size - sizeof(FILE_TRAILER_T));

                // check if file crc is correct
                if (calculated_crc == t->crc)
                {
                    best_sequence = t->file_sequence;
                    *trailer = t;
                    err = 0;
                }
            }
        }
    }

    return(err);
}


/*!
 * \brief find the boot image using minimal software (no file system functions, no dma based crc)
 * \details This is the boot method of last resort. This code lives at the start of flash and should
 *          NEVER be erased!
 *          This function will find the boot image if it exists in flash.  It is only
 *          relied upon used when all else fails due to flash corruption.
 * \param[in]   filename     name to find
 * \param[in]   fid          fid to find or FS_INVALID_FID, if valid the fid is used instead of the name
 * \param[out]  trailer      pointer to file trailer
 * \return 0 on success
 */
int pluto_find_boot_image(uint32_t *physical_flash_offset)
{
    int err = -1;
    int i;
    FILE_TRAILER_T *boot_trailer = NULL;
    FILE_TRAILER_T *exe_trailer = NULL;
    char *exe = NULL;
    char exe_filename[16];
    char *boot_text = NULL;

    // for boot.txt file
    if (!pluto_boot_find_file("boot.txt", &boot_trailer) && boot_trailer)
    {
        // extract executable filename from the first line of the file
        boot_text = (char *)boot_trailer + sizeof(FILE_TRAILER_T) - boot_trailer->file_size;

        for(i=0; i<16; i++)
        {
            exe_filename[i] = boot_text[i];

            if ((exe_filename[i] == '\n') || (i > (boot_trailer->file_size - sizeof(FILE_TRAILER_T))))
            {
                exe_filename[i] = 0;
                break;
            }
        }

        // look for file name of file to boot on first line of boot.txt file
        if (!pluto_boot_find_file(exe_filename, &exe_trailer) && exe_trailer)
        {
            exe = (char *)exe_trailer + sizeof(FILE_TRAILER_T) - exe_trailer->file_size;
            *physical_flash_offset = (u_int32_t)((char *)exe - FS_BASE);
            err = 0;
        }
    }
    
    return(err);
}

/**
 * Pure software CRC32 engine. Operates bit-by-bit to avoid 1KB lookup tables.
 * Leaves the RP2350 DMA and Sniffer hardware completely untouched.
 */
u32_t __attribute__((section(".boot_entry"))) picofs_software_crc32(u8_t *ptr, u32_t length)
{
    u32_t crc = 0xFFFFFFFF;
    //volatile u8_t *ptr = (volatile u8_t *)(RAW_FLASH_BASE + physical_start);

    for (u32_t i = 0; i < length; i++)
    {
        crc ^= ptr[i];
        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xEDB88320; // Standard reversed polynomial
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

// /**
//  * Immutable boot recovery scan loop. Placed permanently in Sector 0.
//  */
// void __attribute__((section(".boot_entry"))) picofs_trailer_scan_and_boot(void)
// {
//     // Safety check: Bypass if an app is already handling its running loop
//     volatile uint32_t *scratch0 = (volatile uint32_t *)0x400D800C;
//     if (*scratch0 == 0xFEEDC0DE || *scratch0 == 0xDEADD00D)
//     {
//         return;
//     }

//     // Scan physical flash block endpoints.
//     // Start at offset 64KB to leave this bootloader block completely untouched.
//     for (uint32_t block_base = SCAN_GRID_STEP; block_base < FLASH_CHIP_SIZE; block_base += SCAN_GRID_STEP)
//     {
//         volatile uint32_t *vector_table = (volatile uint32_t *)(RAW_FLASH_BASE + block_base);
//         uint32_t target_stack = vector_table[0];
//         uint32_t target_handler = vector_table[1];

//         // Basic Vector validation: Ensure pointers match valid RAM and XIP windows
//         if (target_stack < 0x20000000 || target_stack > 0x20080000 ||
//             target_handler < 0x10000000 || target_handler > 0x11000000)
//         {
//             continue;
//         }

//         // Trace out to find the trailer matching this 64KB block's structural endpoint
//         uint32_t theoretical_trailer_ptr = RAW_FLASH_BASE + block_base + SCAN_GRID_STEP - sizeof(FILE_TRAILER_T);
//         volatile FILE_TRAILER_T *trailer = (volatile FILE_TRAILER_T *)theoretical_trailer_ptr;

//         while ((uint32_t)trailer < (RAW_FLASH_BASE + FLASH_CHIP_SIZE))
//         {
//             if (trailer->magic_number[0] == 'p' &&
//                 trailer->magic_number[1] == 'f' &&
//                 trailer->magic_number[2] == 's')
//             {
//                 break; // Valid structural endpoint identified
//             }
//             trailer = (volatile FILE_TRAILER_T *)((uint32_t)trailer + SCAN_GRID_STEP);
//         }

//         if ((uint32_t)trailer >= (RAW_FLASH_BASE + FLASH_CHIP_SIZE))
//         {
//             continue;
//         }

//         u8_t status = trailer->file_status;
//         if ((status & STS_EXECUTABLE) && !(status & STS_DELETED))
//         {
//             u32_t file_length = trailer->file_size;

//             // Safety guard: validate size parameters before iterating memory
//             if (file_length > 0 && file_length < FLASH_CHIP_SIZE)
//             {
//                 // Verify the file content using the software CRC32 engine
//                 u32_t computed_crc = picofs_software_crc32(block_base, file_length);

//                 if (computed_crc != trailer->crc)
//                 {
//                     continue; // CRC mismatch; skip corrupt file and continue scanning
//                 }
//             }
//             else
//             {
//                 continue;
//             }

//             // 3. CRC Pass Verified. Shut down background ticks and map the target app.
//             SYSTICK_CTRL = 0;

//             // Remap virtual 0x10000000 cleanly to this physical 64KB grid boundary block.
//             QMI_ATRANS0 = (4 << 22) | (block_base >> 12);

//             // Execute explicit memory fences to force a QMI router table pipeline refresh
//             __asm volatile("dsb sy" : : : "memory");
//             __asm volatile("isb sy" : : : "memory");

//             // Redirect the Vector Table Offset Register (VTOR) to XIP Space
//             SCB_VTOR = 0x10000000;

//             uint32_t target_entry = target_handler | 1;

//             // Reset Stack Pointer, open core interrupts, and branch into the target application
//             __asm volatile(
//                 "msr msp, %0\n"
//                 "cpsie i\n"
//                 "bx %1\n"
//                 : : "r"(target_stack), "r"(target_entry) : "memory");

//             while (1);
//         }
//     }
// }