/**
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"

extern MessageBufferHandle_t xMessageBuffer;

int shell_queue_heap(void)
{
    SM_GENERIC_FILE_T msg;


    msg.message_id = SHELL_MSG_HEAP;

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);


    return(0);
}

/*!
 * \brief print heap statistics to the shell
 *
 * \return nothing
 */
int shell_do_heap(SM_GENERIC_FILE_T *msg)
{
    HeapStats_t xHeapStats;

    vPortGetHeapStats(&xHeapStats);

    shell_printf("Available Space:               %u bytes\n", xHeapStats.xAvailableHeapSpaceInBytes);
    shell_printf("Largest Free Block:            %u bytes\n", xHeapStats.xSizeOfLargestFreeBlockInBytes);
    shell_printf("Smallest Free Block:           %u bytes\n", xHeapStats.xSizeOfSmallestFreeBlockInBytes);
    shell_printf("Number of Free Blocks:         %u\n", xHeapStats.xNumberOfFreeBlocks);
    shell_printf("Historical Minimum Free Bytes: %u\n", xHeapStats.xMinimumEverFreeBytesRemaining);

    return(0);
}
