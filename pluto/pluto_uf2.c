#include <fcntl.h>
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "pluto.h"
#include "picofs.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "lwip/apps/http_client.h"

// Define the offset in flash memory where the download should begin writing.
// Ensure your linker script maps the secondary app space to start here.
#define OTA_FLASH_TARGET_OFFSET (1024 * 1024) // 1MB Offset 

#define UF2_MAGIC_START0 0x0A324655
#define UF2_MAGIC_START1 0x9E5D5157
#define UF2_MAGIC_END    0x0AB41357



// Buffer to store fractional data chunks arriving across network packets
// static uint8_t stream_buffer[512];
// static size_t stream_buffer_idx = 0;
// static uint32_t last_erased_sector_addr = 0xFFFFFFFF;

// Process a completely reconstructed 512-byte UF2 block
void pluto_write_uf2_block(int fd, const uf2_block_t *block) 
{
    int err = 0;

    // Validate block integrity signatures
    if (block->magic_start0 != UF2_MAGIC_START0 || 
        block->magic_start1 != UF2_MAGIC_START1 || 
        block->magic_end != UF2_MAGIC_END) 
    {
        return; // Ignore corrupt or malformed blocks
    }

    // Skip payload blocks meant for other architectures (e.g. RP2040)
    // RP2350-ARM-S Family ID is 0xe48bff56; RP2350-RISCV is 0x78f240e3
    if ((block->flags & 0x00002000) && 
        (block->family_id != 0xe48bff56 && block->family_id != 0x78f240e3)) 
    {
        return; 
    }

    // Flash the 256-byte payload (adjust according to standard UF2 block sizes)
    if (block->payload_size == 256) 
    {
        // we presume the UF2 is targeted at XIP_BASE and find the offset from XIP_BASE
        uint32_t target_offset = block->target_addr - XIP_BASE;

        // move to the offset withing the file
        err = picofs_seek(fd, target_offset, SEEK_SET);

        if (!err)
        {
            err = picofs_write(fd, (char *)block->data, 256);
        }
    }
}

