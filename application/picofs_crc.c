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

// TODO - prune this list of includes
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

// #include "weather.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"
// #include "led_strip.h"
#include "udp.h"
// #include "message.h"
// #include "message_defs.h"
// #include "powerwall.h"
#include "shelly.h"
#include "discovery_task.h"
#include "picofs.h"




//#define DEBUG_UDP_MESSAGES

//#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)





//prototypes


// external variables
extern u32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];

//static variables
// FILE_METRICS_T purge_list[FS_NUM_FID]; 
 
/*!
 * \brief template 
 *
 * \param fd              file descriptor
 * \param disable_purge   do not purge duplicate filenames (only used when already executing a purge)
 * \return 0 on success
 */






// #include "FreeRTOS.h"
// #include "semphr.h"
// #include "hardware/dma.h"
// #include "hardware/irq.h"
// #include "hardware/structs/xip_ctrl.h"

static SemaphoreHandle_t xSnifferMutex = NULL;
static SemaphoreHandle_t xDmaDoneSemaphore = NULL;
static int g_claimed_dma_chan = -1;

static void __not_in_flash_func(dma_crc_isr)() {
    if (g_claimed_dma_chan >= 0) {
        dma_hw->ints0 = (1u << g_claimed_dma_chan);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xDmaDoneSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void init_crc_subsystem(void) {
    if (xSnifferMutex == NULL) {
        xSnifferMutex = xSemaphoreCreateMutex();
        xDmaDoneSemaphore = xSemaphoreCreateBinary();
        irq_set_exclusive_handler(DMA_IRQ_0, dma_crc_isr);
        irq_set_enabled(DMA_IRQ_0, true);
    }
}

uint32_t picofs_calculate_crc32(const uint8_t *data_ptr, size_t byte_length) {
    uint32_t crc = 0xffffffff;
    bool is_flash = ((uint32_t)data_ptr >= 0x10000000 && (uint32_t)data_ptr < 0x20000000);

    if (xSemaphoreTake(xSnifferMutex, portMAX_DELAY) == pdTRUE) {
        
        // --- FIX FOR RANDOM FIRST-RUN CHECKSUM ---
        if (is_flash) {
            // Force reset the hardware stream controller counter to kill any active fetches
            xip_ctrl_hw->stream_ctr = 0;
            
            // Drain the hardware FIFO queue entirely to destroy stale, uninitialized data
            while (xip_ctrl_hw->stream_fifo != 0) {
                __compiler_memory_barrier();
            }
        }
        // ------------------------------------------

        g_claimed_dma_chan = dma_claim_unused_channel(true);
        dma_channel_config c = dma_channel_get_default_config(g_claimed_dma_chan);

        // Required: Byte-by-byte transfer handles completely unaligned bounds safely
        channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
        channel_config_set_write_increment(&c, false);

        const void *read_address;

        if (is_flash) {
            channel_config_set_read_increment(&c, false); 
            channel_config_set_dreq(&c, DREQ_XIP_STREAM);
            read_address = (const void *)&xip_ctrl_hw->stream_fifo;
        } else {
            channel_config_set_read_increment(&c, true);
            channel_config_set_dreq(&c, DREQ_FORCE);
            read_address = data_ptr;
        }

        // Configure the shared hardware Sniffer
        channel_config_set_sniff_enable(&c, true);
        dma_hw->sniff_data = 0xffffffff; 
        dma_sniffer_enable(g_claimed_dma_chan, 0x0, true); // Mode 0x0 = IEEE 802.3 CRC32

        channel_config_set_irq_quiet(&c, false);
        dma_channel_set_irq0_enabled(g_claimed_dma_chan, true);
        xSemaphoreTake(xDmaDoneSemaphore, 0); // Purge historical binary semaphores

        static uint8_t dummy_sink_8; // 8-bit sink match for byte-sized transfers
        dma_channel_configure(
            g_claimed_dma_chan, &c,
            &dummy_sink_8, 
            read_address, 
            byte_length, // Always use exact byte count
            true // Safely spin up the DMA listener first
        );

        if (is_flash) {
            // Kick off the actual physical QSPI Flash burst read only AFTER the DMA is open
            xip_ctrl_hw->stream_addr = (uint32_t)data_ptr;
            xip_ctrl_hw->stream_ctr  = byte_length;
        }

        if (xSemaphoreTake(xDmaDoneSemaphore, portMAX_DELAY) == pdTRUE) {
            crc = dma_sniffer_get_data_accumulator();
        }

        dma_sniffer_disable();
        dma_channel_set_irq0_enabled(g_claimed_dma_chan, false);
        dma_channel_unclaim(g_claimed_dma_chan);
        g_claimed_dma_chan = -1;
        
        xSemaphoreGive(xSnifferMutex);
    }
    
    return ~crc;
}
