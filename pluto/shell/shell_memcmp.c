/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"


extern MessageBufferHandle_t xMessageBuffer;

// prototypes
void compare_memory(const void *buf1, const void *buf2, size_t len, size_t max_lines);

int shell_queue_memcmp(char *dst, char *src, char *len)
{
    SM_GENERIC_DST_SRC_LEN_T msg;

    if (dst && src && len)
    {    
        msg.message_id = SHELL_MSG_MEMCMP;
        STRNCPY(msg.dst, dst, sizeof(msg.dst));
        STRNCPY(msg.src, src, sizeof(msg.src));
        STRNCPY(msg.len, len, sizeof(msg.len));        

        xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);
    }
    
    return(0);
}


/*!
 * \brief display a details of a file
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_memcmp(SM_GENERIC_DST_SRC_LEN_T *msg)
{
    int destination = 0;
    int source = 0;
    int length = 0;

    if (!msg || !(msg->dst[0]) || !(msg->src[0]) || !(msg->len[0]))
    {
        // error missing parameter
        return(-1);
    }

    // TODO: make smarter, decimal, hex, filename
    sscanf(msg->dst, "%x", &destination);
    sscanf(msg->src, "%x", &source);
    sscanf(msg->len, "%d", &length);

    compare_memory((void *)destination, (void *)source, length, 1000);
    
    return(0);
}

/*!
 * \brief compare memory blocks
 *
 * \param[in]  src  source
 * \param[in]  dst  destination
 * \param[in]  len  length 
 * \return nothing
 */
int shell_memcmp(char *src_space_dst_space_len)
{
    FILE *filePointer;
    char buffer[256];
    int i, j, k;
    char src[16];
    char dst[16];
    char len[16];    

    src[0] = 0;
    dst[0] = 0;
    len[0] = 0;    

    // extract source
    for(i=0; i<strlen(src_space_dst_space_len); i++)
    {
        if ((src_space_dst_space_len[i] == 0) ||
            (!isprint(src_space_dst_space_len[i])) ||
            (src_space_dst_space_len[i] == ' '))
        {
            src[i] = 0;
            break;
        }

        src[i] = src_space_dst_space_len[i];
        src[i+1] = 0;
    }

    // skip space
    if (src_space_dst_space_len[i] == ' ')
    {
        i++;
    }

    // extract destination
    j = 0;
    for(; i<strlen(src_space_dst_space_len); i++)
    {
        if ((src_space_dst_space_len[i] == 0) ||
            (!isprint(src_space_dst_space_len[i])) ||
            (src_space_dst_space_len[i] == ' '))
        {
            dst[j] = 0;
            break;
        }

        dst[j++] = src_space_dst_space_len[i];
        dst[j] = 0;
    }

    // skip space
    if (src_space_dst_space_len[i] == ' ')
    {
        i++;
    }

    // extract length
    k = 0;
    for(; i<strlen(src_space_dst_space_len); i++)
    {
        if ((src_space_dst_space_len[i] == 0) ||
            (!isprint(src_space_dst_space_len[i])) ||
            (src_space_dst_space_len[i] == ' '))
        {
            len[k] = 0;
            break;
        }

        len[k++] = src_space_dst_space_len[i];
        len[k] = 0;
    }

    if ((strlen(src) > 2) && (strlen(dst) > 2) && (strlen(len) > 0))
    {
        shell_queue_memcmp(dst, src, len);
    }

    return 0;
}



/**
 * @brief Compares two memory blocks and prints differences side-by-side.
 *        Condenses matching ranges and stops printing after max_lines is reached.
 * 
 * @param buf1      Pointer to the first memory block.
 * @param buf2      Pointer to the second memory block.
 * @param len       Number of bytes to compare.
 * @param max_lines Maximum number of output lines to print before breaking.
 */
void compare_memory(const void *buf1, const void *buf2, size_t len, size_t max_lines) 
{
    const uint8_t *p1 = (const uint8_t *)buf1;
    const uint8_t *p2 = (const uint8_t *)buf2;
    
    size_t diff_count = 0;
    size_t lines_printed = 0;
    
    // Tracking for consecutive matching runs
    size_t match_start = 0;
    int in_match_run = 0;

    shell_printf("Offset     | Buf 1 (ASCII) | Buf 2 (ASCII)\n");
    shell_printf("-----------+---------------+---------------\n");

    for (size_t i = 0; i < len; i++) {
        if (p1[i] == p2[i]) {
            if (!in_match_run) {
                match_start = i;
                in_match_run = 1;
            }
            continue;
        }

        // We found a difference. If we were in a match run, flush it to the screen.
        if (in_match_run) {
            size_t match_len = i - match_start;
            if (lines_printed < max_lines) {
                shell_printf("0x%08X  | [... %zu bytes match ...]\n", (unsigned int)match_start, match_len);
                lines_printed++;
            }
            in_match_run = 0;
        }

        // Check if we hit our output limit before printing the mismatch row
        if (lines_printed >= max_lines) {
            shell_printf("[... Output truncated: Reached maximum line limit of %zu ...]\n", max_lines);
            // Continue loop without printing to accurately finish tracking total diff_count
            diff_count += (len - i); // Quick skip since we only need the count now
            for (size_t j = i; j < len; j++) {
                if (p1[j] != p2[j]) diff_count++;
            }
            break;
        }

        // Prepare readable ASCII markers
        char c1 = isprint(p1[i]) ? (char)p1[i] : '.';
        char c2 = isprint(p2[i]) ? (char)p2[i] : '.';

        shell_printf("0x%08X |   %02X  ('%c')   |   %02X  ('%c')\n", (unsigned int)i, p1[i], c1, p2[i], c2);
        lines_printed++;
        diff_count++;
    }

    // Handle trailing match run if the buffers ended on a match
    if (in_match_run && lines_printed < max_lines) {
        size_t match_len = len - match_start;
        shell_printf("0x%08X  | [... %zu bytes match ...]\n", (unsigned int)match_start, match_len);
    }

    shell_printf("-----------+---------------+---------------\n");
    shell_printf("Total mismatches: %zu bytes out of %zu\n", diff_count, len);
}
