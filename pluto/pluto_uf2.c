/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pluto.h"
#include "picofs.h"
#include "utility.h"

// defines
#define UF2_MAGIC_START0   0x0A324655
#define UF2_MAGIC_START1   0x9E5D5157
#define UF2_MAGIC_END      0x0AB16F30
#define KLUDGE_ERRATA_E10  0xe48bff57

// process a completely reconstructed 512-byte UF2 block
void pluto_write_uf2_block(int fd, const uf2_block_t *block) 
{
    int err = 0;

    // Validate block integrity signatures
    if (block->magic_start0 != UF2_MAGIC_START0 || 
        block->magic_start1 != UF2_MAGIC_START1 || 
        block->magic_end != UF2_MAGIC_END) 
    {
        // Ignore corrupt or malformed blocks
        printf("pluto_write_uf2_block: Ignore corrupt or malformed blocks\n");
        return; 
    }

    // Skip payload blocks meant for other architectures (e.g. RP2040)
    // Valid RP2350 Family IDs cover both Secure (S) and Non-secure (NS) variants
    if (block->flags & 0x00002000) 
    {
        // ignore kludge block used by the ROM bootloader
        if (block->family_id == KLUDGE_ERRATA_E10)
        {
            printf("pluto_write_uf2_block: Ignore kludge block used for the ERRATA E10 fix\n");
            return;        
        }    

        if (block->family_id != 0xe48bff56 && // RP2350 ARM Secure
            block->family_id != 0xe48bff59 && // RP2350 ARM Non-secure
            block->family_id != 0x78f240e3 && // RP2350 RISC-V Secure
            block->family_id != 0x78f240e4)   // RP2350 RISC-V Non-secure
        {
            printf("pluto_write_uf2_block: Ignore blocks for different CPU [%0x]\n", block->family_id);
            return; 
        }
    }

    // Flash the 256-byte payload (adjust according to standard UF2 block sizes)
    if (block->payload_size == 256) 
    {
        // we presume the UF2 is targeted at XIP_BASE and find the offset from XIP_BASE
        uint32_t target_offset = block->target_addr - XIP_BASE;

        // move to the offset withing the file
        err = picofs_lseek(fd, target_offset, SEEK_SET);

        if (err != -1)
        {
            err = picofs_write(fd, (char *)block->data, 256);
        }
    }
}

