/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>


#include "hardware/pio.h"
#include "hardware/clocks.h"
// #include "generated/ws2812.pio.h"

// Prune this list of includes
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>


#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "semphr.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/xip_ctrl.h" // Required for XIP Stream control


#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"

#include "udp.h"

#include "shelly.h"
#include "discovery_task.h"
#include "picofs.h"


//prototypes


// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern SemaphoreHandle_t crc_mutex;
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

// static variables
static volatile TaskHandle_t xCrcTaskToNotify = NULL;
static volatile int g_allocated_dma_chan = -1;
static volatile uint32_t g_dummy_dest = 0;


#ifdef BLOCKING_CRC_FUNCTION
/*!
 * \brief Computes standard IEEE 802.3 CRC-32 (ethernet polynomial 0x04C11DB7, bit-reversed)
 *
 * \param src   data to use for crc calculation
 * \param len   length of data
 * \return 0 on success
 */
uint32_t picofs_calculate_crc32(const uint8_t *src, size_t len) {   //<=== THIS WORKS!
    // 1. Claim a free DMA channel
    int dma_chan = dma_claim_unused_channel(true);
    
    // 2. Configure the hardware sniffer block
    // Mode 0x0 is the standard IEEE 802.3 CRC-32 polynomial
    dma_sniffer_enable(dma_chan, 0x0, true);
    
    // Seed value: Standard CRC-32 initializes with 0xFFFFFFFF
    dma_hw->sniff_data = 0xFFFFFFFF;

    // 3. Configure the DMA channel parameters
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    
    // Enable the sniffer for this specific DMA pipeline channel
    channel_config_set_sniff_enable(&c, true);
    
    // CRITICAL: 8-bit size reads safely from any memory offset (odd/even) in Flash or RAM
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    
    // Increment source pointer, lock destination pointer to the dummy target
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    // FIX: Provide a safe, isolated SRAM 32-bit register target.
    // We point to a dedicated local volatile variable rather than the sniffer register itself
    // to prevent the bus from feeding the sniffer register into itself.
    volatile uint32_t dummy_dest = 0;

    // 4. Set up and immediately start the transfer
    dma_channel_configure(
        dma_chan,
        &c,
        (void*)&dummy_dest, // Target destination (safe SRAM sink)
        src,                // Source pointer (RAM or Flash, arbitrary byte alignment)
        len,                // Total bytes to process
        true                // Start immediately
    );

    // 5. Wait for the hardware block to finish
    dma_channel_wait_for_finish_blocking(dma_chan);

    // 6. Clean up resources to prevent hardware leaks
    dma_sniffer_disable();
    dma_channel_unclaim(dma_chan);

    // 7. Extract the result
    // Standard CRC-32 outputs require a final bitwise inversion (XOR 0xFFFFFFFF)
    return dma_hw->sniff_data ^ 0xFFFFFFFF;
}

#else // Non-blocking CRC function

/*!
 * \brief Hardware Interrupt Service Routine for CRC calculation
 *
 * \return 0 on success
 */
void __not_in_flash_func(dma_crc_irq_handler)() {
    // Clear the interrupt flag on the assigned channel to stop re-triggering
    dma_hw->ints0 = (1u << g_allocated_dma_chan);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xCrcTaskToNotify != NULL) {
        // Unblock the waiting task using a direct-to-task notification
        vTaskNotifyGiveFromISR(xCrcTaskToNotify, &xHigherPriorityTaskWoken);
        xCrcTaskToNotify = NULL;
    }

    // Force a context switch if the unblocked task has a higher priority
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/*!
 * \brief Computes standard IEEE 802.3 CRC-32 (ethernet polynomial 0x04C11DB7, bit-reversed). Thread-safe, non-blocking asynchronous hardware CRC function.
 *
 * \param src   data to use for crc calculation
 * \param len   length of data
 * \return CRC on success
 */
uint32_t picofs_calculate_crc32(const uint8_t *src, size_t len) 
{
    if (xSemaphoreTake(crc_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {    
        // Save current task reference so the ISR knows who to wake up
        xCrcTaskToNotify = xTaskGetCurrentTaskHandle();

        // Claim a free DMA channel dynamically
        g_allocated_dma_chan = dma_claim_unused_channel(true);
        
        // Configure hardware sniffer (Mode 0x0 = Standard IEEE 802.3 CRC-32)
        dma_sniffer_enable(g_allocated_dma_chan, 0x0, true);
        dma_hw->sniff_data = 0xFFFFFFFF; // Seed register

        // Configure DMA parameters
        dma_channel_config c = dma_channel_get_default_config(g_allocated_dma_chan);
        channel_config_set_sniff_enable(&c, true);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_8); // Byte alignment safe
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);

        // Reset dummy memory target
        g_dummy_dest = 0;

        // Hook up the IRQ hardware line
        dma_channel_set_irq0_enabled(g_allocated_dma_chan, true);
        
        // Bind shared DMA IRQ0 line to our specific handler function
        irq_set_exclusive_handler(DMA_IRQ_0, dma_crc_irq_handler);
        irq_set_enabled(DMA_IRQ_0, true);

        // Launch the DMA operation asynchronously
        dma_channel_configure(
            g_allocated_dma_chan,
            &c,
            (void*)&g_dummy_dest, 
            src,                  
            len,                  
            true // Trigger execution immediately
        );

        // YIELD THE CPU: The task sleeps block until notified by ISR
        // Consumes 0% CPU cycles while calculating large chunks or reading slow flash
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Cleanup and free up hardware allocations
        dma_channel_set_irq0_enabled(g_allocated_dma_chan, false);
        irq_set_enabled(DMA_IRQ_0, false);
        dma_sniffer_disable();
        
        uint32_t final_crc = dma_hw->sniff_data ^ 0xFFFFFFFF; // Inverse post-process

        dma_channel_unclaim(g_allocated_dma_chan);
        g_allocated_dma_chan = -1;

        xSemaphoreGive(crc_mutex); 

        return final_crc;
    }
    else
    {
        return(0xffffffff);
    }
    
}

#endif