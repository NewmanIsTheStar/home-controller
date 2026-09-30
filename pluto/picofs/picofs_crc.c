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
#include "pico/bit_ops.h"

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

#include "utility.h"
#include "config.h"
#include "system_config.h"
#include "application_config.h"
#include "watchdog.h"
#include "pluto.h"

#include "udp.h"

#include "shelly.h"
#include "discovery_task.h"
#include "picofs.h"

// The reflected polynomial for IEEE 802.3 CRC-32
#define CRC32_POLY 0xEDB88320

//prototypes


// external variables
extern u32_t unix_time;
extern APP_CONFIG_T config;
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



/*!
 * \brief Hardware Interrupt Service Routine for CRC calculation
 *
 * \return 0 on success
 */
void __not_in_flash_func(dma_crc_irq_handler)() 
{
    // Clear the interrupt flag on the assigned channel to stop re-triggering
    dma_hw->ints0 = (1u << g_allocated_dma_chan);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xCrcTaskToNotify != NULL) 
    {
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
    //uint32_t software_crc;  // used to verify the output of the DMA SNIFFER

    if (xSemaphoreTake(crc_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {    
        // Save current task reference so the ISR knows who to wake up
        xCrcTaskToNotify = xTaskGetCurrentTaskHandle();

        // Claim a free DMA channel dynamically
        g_allocated_dma_chan = dma_claim_unused_channel(true);
        
        // dma sniffer setup
        dma_sniffer_enable(g_allocated_dma_chan, 0x1, true);

        dma_hw->sniff_ctrl = (dma_hw->sniff_ctrl & ~((0xfu << DMA_SNIFF_CTRL_CALC_LSB) | 
                                                    DMA_SNIFF_CTRL_OUT_INV_BITS | 
                                                    DMA_SNIFF_CTRL_OUT_REV_BITS |
                                                    DMA_SNIFF_CTRL_BSWAP_BITS))
                            | (0x1u << DMA_SNIFF_CTRL_CALC_LSB)
                            | DMA_SNIFF_CTRL_BSWAP_BITS; // Keep only byte-swapping active

        dma_hw->sniff_data = 0xFFFFFFFF; // Seed

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
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Cleanup and free up hardware allocations
        dma_channel_set_irq0_enabled(g_allocated_dma_chan, false);
        irq_set_enabled(DMA_IRQ_0, false);
        dma_sniffer_disable();
        
        // read the register raw
        uint32_t raw_reg = dma_hw->sniff_data; 

        // hardware-accelerated 32-bit bit reversal
        uint32_t reversed = __rev(raw_reg);

        // post-conditioning: XOR the final remainder with all 1s
        uint32_t final_crc = ~reversed;

        dma_channel_unclaim(g_allocated_dma_chan);
        g_allocated_dma_chan = -1;
        xSemaphoreGive(crc_mutex); 

        // // software verification
        // software_crc = picofs_calculate_crc32_software(src, len);

        // if (software_crc != final_crc)
        // {
        //     printf("bad DMA CRC %0x [raw = %0x] vs SW CRC %0x\n", final_crc, raw_reg, software_crc);
        // }

        return final_crc;
        //return software_crc;
    }
    else
    {
        return(0xffffffff);
    }
}


/*!
 * \brief Computes standard IEEE 802.3 CRC-32 (ethernet polynomial 0x04C11DB7, bit-reversed) 
 *
 * \param src   data to use for crc calculation
 * \param len   length of data
 * \return 0 on success
 */
uint32_t picofs_calculate_crc32_blocking(const uint8_t *src, size_t len)   // NB this version blocks the CPU until completed!
{   
    // claim a free DMA channel
    int dma_chan = dma_claim_unused_channel(true);
    
    // configure the hardware sniffer block
    // Mode 0x0 is the standard IEEE 802.3 CRC-32 polynomial
    dma_sniffer_enable(dma_chan, 0x0, true);
    
    // Seed value: Standard CRC-32 initializes with 0xFFFFFFFF
    dma_hw->sniff_data = 0xFFFFFFFF;

    // configure the DMA channel parameters
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

    // set up and immediately start the transfer
    dma_channel_configure(
        dma_chan,
        &c,
        (void*)&dummy_dest, // Target destination (safe SRAM sink)
        src,                // Source pointer (RAM or Flash, arbitrary byte alignment)
        len,                // Total bytes to process
        true                // Start immediately
    );

    // wait for the hardware block to finish
    dma_channel_wait_for_finish_blocking(dma_chan);

    // clean up resources to prevent hardware leaks
    dma_sniffer_disable();
    dma_channel_unclaim(dma_chan);

    // extract the result
    // Standard CRC-32 outputs require a final bitwise inversion (XOR 0xFFFFFFFF)
    return dma_hw->sniff_data ^ 0xFFFFFFFF;
}


/**
 * @brief Multiplies a row vector by a GF(2) matrix.
 */
static uint32_t gf2_matrix_times(const uint32_t *matrix, uint32_t vector) 
{
    uint32_t sum = 0;
    while (vector) 
    {
        if (vector & 1) 
        {
            sum ^= *matrix;
        }
        vector >>= 1;
        matrix++;
    }
    return sum;
}

/**
 * @brief Squares a 32x32 matrix over GF(2).
 */
static void gf2_matrix_square(uint32_t *square, const uint32_t *matrix) 
{
    for (int n = 0; n < 32; n++) 
    {
        square[n] = gf2_matrix_times(matrix, matrix[n]);
    }
}

/**
 * @brief Combines two IEEE 802.3 CRC-32 hashes.
 * 
 * @param crc1   The fully-conditioned CRC-32 of Block A (pre/post-XORed with 0xFFFFFFFF)
 * @param crc2   The fully-conditioned CRC-32 of Block B (pre/post-XORed with 0xFFFFFFFF)
 * @param len2   The length of Block B in BYTES
 * @return uint32_t The exact combined CRC-32 of (Block A concatenated with Block B)
 */
uint32_t picofs_combine_crc32(uint32_t crc1, uint32_t crc2, size_t len2) 
{
    // If block B is empty, the total CRC is just CRC A
    if (len2 == 0) 
    {
        return crc1;
    }

    uint32_t matrix[32];
    uint32_t intermediate_matrix[32];

    // 1. Construct the transformation matrix for 1 zero shift-bit
    matrix[0] = CRC32_POLY;
    uint32_t row = 1;
    for (int n = 1; n < 32; n++) 
    {
        matrix[n] = row;
        row <<= 1;
    }

    // 2. Scale the matrix from 1-bit shift to 1-byte shift (8 bits)
    // By squaring the matrix 3 times (2^3 = 8)
    gf2_matrix_square(intermediate_matrix, matrix); // 2 bits
    gf2_matrix_square(matrix, intermediate_matrix); // 4 bits
    gf2_matrix_square(intermediate_matrix, matrix); // 8 bits (1 byte)

    // 3. Repeated squaring technique for len2 bytes (O(log N))
    // We utilize 'matrix' to stack up the power shifts
    for (int n = 0; n < 32; n++) 
    {
        matrix[n] = intermediate_matrix[n];
    }

    while (len2 > 0) 
    {
        // If the lowest bit of length is set, apply the current matrix shift
        if (len2 & 1) 
        {
            crc1 = gf2_matrix_times(matrix, crc1);
        }
        len2 >>= 1;
        if (len2 == 0) 
        {
            break;
        }
        // Square the matrix for the next bit position power of 2
        gf2_matrix_square(intermediate_matrix, matrix);
        for (int n = 0; n < 32; n++) 
        {
            matrix[n] = intermediate_matrix[n];
        }
    }

    // 4. Combine by XORing directly with the CRC of block B
    return crc1 ^ crc2;
}



// Precomputed look-up table for the standard IEEE 802.3 CRC-32 polynomial (0xEDB88320 reversed)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/**
 * @brief Computes the standard IEEE 802.3 CRC-32 over a buffer.
 * 
 * @param data Pointer to the input data byte array.
 * @param length Size of the data in bytes.
 * @return uint32_t The final computed checksum (pre and post-conditioned).
 */
uint32_t picofs_calculate_crc32_software(const uint8_t *data, size_t length) 
{
    // pre-conditioning: Initialize the remainder to all 1s (0xFFFFFFFF)
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) 
    {
        uint8_t byte = data[i];
        // index into the look-up table using the current CRC LSB combined with the byte
        crc = (crc >> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
    }

    // post-conditioning: XOR the final remainder with all 1s
    return crc ^ 0xFFFFFFFF;
}
