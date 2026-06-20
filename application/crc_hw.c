#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"

/* Notes
  The Global Sniffer Bottleneck: The RP2040 and RP2350 microcontrollers contain only one global sniffer engine.
  While you have up to 12 distinct DMA channels available, only one channel can utilize hardware-driven CRC calculations
  at any given time. Avoid concurrent asynchronous processes utilizing the sniffer simultaneously without mutex locks.
  Alignment Concerns: When computing CRCs across memory blocks, mismatching your transfer_data_size setting (e.g., using
  DMA_SIZE_32 on a data buffer that isn't perfectly a multiple of 4 bytes) will cause invalid bounds processing.
  Keep data sizes matching data array types.
*/

uint32_t calculate_buffer_crc32(const uint8_t *data, size_t length) {
    // 1. Claim a free DMA channel
    int dma_chan = dma_claim_unused_channel(true);
    
    // 2. Configure the channel to read sequentially but write to a single location
    volatile uint8_t dummy_dest; 
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8); // Byte-by-byte
    channel_config_set_read_increment(&cfg, true);           // Advance through source buffer
    channel_config_set_write_increment(&cfg, false);         // Force single target write
    channel_config_set_sniff_enable(&cfg, true);             // route through Sniffer
    
    // 3. Set up the Hardware Sniffer for CRC-32 (Normal Mode 0x0)
    dma_sniffer_enable(dma_chan, 0x0, true);
    dma_sniffer_set_data_accumulator(0xFFFFFFFF); // Seed value
    
    // 4. Configure and trigger the transfer immediately
    dma_channel_configure(
        dma_chan,
        &cfg,
        &dummy_dest, // Destination address
        data,        // Source address
        length,      // Number of transfers
        true         // Start immediately
    );
    
    // 5. Wait for the hardware to process the memory block
    dma_channel_wait_for_finish_blocking(dma_chan);
    
    // 6. Harvest result and free up hardware resources
    uint32_t final_crc = dma_sniffer_get_data_accumulator();
    
    // Standard IEEE 802.3 requires post-inversion (XOR out with 0xFFFFFFFF)
    final_crc ^= 0xFFFFFFFF; 
    
    dma_sniffer_disable();
    dma_channel_unclaim(dma_chan);
    
    return final_crc;
}
