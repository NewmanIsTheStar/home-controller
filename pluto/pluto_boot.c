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
//#include "hardware/rtc.h"
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

#include "ssi.h"
#ifdef USE_GIT_HASH_AS_VERSION
#include "githash.h"
#endif


// uninitialized variable


// external variables
extern u32_t unix_time;
extern APP_CONFIG_T config;
extern WEB_VARIABLES_T web;
extern WORKER_TASK_T worker_tasks[];

// static variables


// prototypes
void __no_inline_not_in_flash_func(remap_and_boot_app)(uint32_t physical_flash_offset);



/*!
 * \brief load the executable addresses into watchdog scratch registers in preparation for the next boot
 *
 * \param none
 *
 * \return 0 if scheduler stops (should never happen)
 */
int pluto_boot_setup(char *exe_physical_addr, char *exe_virtual_addr)
{
    // addresses
    watchdog_hw->scratch[0] = (u32_t)exe_physical_addr;
    watchdog_hw->scratch[1] = (u32_t)exe_virtual_addr;    
    
    // inverted addresses 
    watchdog_hw->scratch[2] = ~(u32_t)exe_physical_addr;
    watchdog_hw->scratch[3] = ~(u32_t)exe_virtual_addr;     

    return(0);
}

/*!
 * \brief load the executable addresses into watchdog scratch registers in preparation for the next boot
 *
 * \param none
 *
 * \return 0 if scheduler stops (should never happen)
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
        printf("pluto_boot_launch: got execuable address %p\n", watchdog_hw->scratch[0]);

        flash_offset = watchdog_hw->scratch[0] - XIP_BASE;

        // clear scratch registers to prevent boot loops
        watchdog_hw->scratch[0] = 0x00;
        watchdog_hw->scratch[1] = 0x01;
        watchdog_hw->scratch[2] = 0x02;
        watchdog_hw->scratch[3] = 0x03;                

        printf("Shifting address translation to offset %0x\n", flash_offset);

        printf("Jumping to executable...\n");
        remap_and_boot_app(flash_offset);
    }
    else
    {
        printf("pluto_boot_launch: nothing ready to launch\n");
        rom_flash_reset_address_trans();
    }
   

    return(0);
}




// #include "pico/stdlib.h"
// #include "hardware/sync.h"
// #include "hardware/watchdog.h"
// #include "hardware/structs/qmi.h" // Crucial header for qmi_hw structure on RP2350

void __no_inline_not_in_flash_func(remap_and_boot_app)(uint32_t physical_flash_offset) {
    // 1. Permanently silence interrupts while we manipulate hardware mappings
    uint32_t ints = save_and_disable_interrupts();
    
    // 2. Disable the Watchdog Timer to prevent background chip resets
    watchdog_disable(); 
    
    // 3. Completely disable the ARM SysTick Timer and its interrupts.
    volatile uint32_t *systick_ctrl = (volatile uint32_t *)0xE000E010;
    *systick_ctrl = 0; 

    // 4. Clear out the primary XIP cache to discard obsolete vector mappings
    flash_flush_cache();

    /* 
     * 5. CORRECTED: Remap the XIP Window via the native QMI Hardware block.
     * On the RP2350, qmi_hw->atrans[0] maps the baseline 0x10000000 execution space.
     * The upper bits dictate size matching. We preserve the active window bits 
     * and inject our specific aligned physical page offset chunk.
     */
    // Clear the current base page mapping bits, keeping sizing/permission flags intact
    uint32_t current_atrans = qmi_hw->atrans[0];
    current_atrans &= 0xFF000000; // Preserve upper size configuration tags
    
    // Inject the new page mapping (physical offset shifted to match the 4KiB grid allocation)
    qmi_hw->atrans[0] = current_atrans | (physical_flash_offset >> 12); 

    // 6. Clean out the cache again to commit the new virtual routing table
    flash_flush_cache();

    // 7. Read the vectors directly from the newly translated virtual window.
    uint32_t *vector_table = (uint32_t *)XIP_BASE; 
    uint32_t stack_pointer = vector_table[0]; 
    uint32_t reset_handler = vector_table[1]; 

    // 8. Enforce ARM Thumb Mode bit on the entry address to avoid a UsageFault
    reset_handler |= 1;

    // 9. Update the hardware Vector Table Offset Register (VTOR)
    volatile uint32_t *vtor_reg = (volatile uint32_t *)0xE000ED08;
    *vtor_reg = XIP_BASE;

    // 10. Reset the Main Stack Pointer to the new application's stack frame
    __asm volatile ("msr msp, %0" : : "r" (stack_pointer) : "memory");

    // 11. Clear PRIMASK to leave the CPU in a clean, raw execution state
    __asm volatile ("cpsie i" : : : "memory");

    // 12. Jump execution directly into the remap binary reset entry vector
    void (*target_entry)(void) = (void (*)(void))reset_handler;
    target_entry();
}
