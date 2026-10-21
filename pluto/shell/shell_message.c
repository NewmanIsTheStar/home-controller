/**
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "watchdog.h"

// Define the total buffer size in bytes 
#define STREAM_BUFFER_SIZE_BYTES    (1024)
#define MAX_MSG_SIZE   (256)



MessageBufferHandle_t xMessageBuffer;

void shell_buffer_init(void) 
{
    // Create the message buffer
    xMessageBuffer = xMessageBufferCreate(STREAM_BUFFER_SIZE_BYTES);
    
    if (xMessageBuffer == NULL) 
    {
        // Source allocation failed (out of FreeRTOS heap)
    }
}


void shell_task(void *params) 
{
    uint8_t ucRxBuffer[MAX_MSG_SIZE];
    size_t xReceivedBytes;

    shell_buffer_init();

    for (;;) 
    {
        xReceivedBytes = xMessageBufferReceive(xMessageBuffer, 
                                               ucRxBuffer, 
                                               MAX_MSG_SIZE, 
                                               30000);

        if (xReceivedBytes > 0) 
        {
            // extract message id
            SHELL_MSG_T *pxMsgId = (SHELL_MSG_T *)ucRxBuffer;

            // process message
            switch (*pxMsgId) 
            {
            case SHELL_MSG_CAT_FILE:
                shell_do_cat((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;
            case SHELL_MSG_HEXDUMP_FILE:
                shell_do_hexdump((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;
            case SHELL_MSG_LIST:
                shell_do_list((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;
            case SHELL_MSG_LIST_ALL:
                shell_do_list_all((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;                
            case SHELL_MSG_STAT:
                shell_do_stat((SM_GENERIC_FILE_T *)ucRxBuffer);
                break; 
            case SHELL_MSG_PAGE_MAP:  
            case SHELL_MSG_PAGE_NUMBERS:
                shell_do_page_map((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;                                   
            case SHELL_MSG_COPY:
                shell_do_copy((SM_GENERIC_DST_SRC_T *)ucRxBuffer);
                break;  
            case SHELL_MSG_MOVE:
                shell_do_move((SM_GENERIC_DST_SRC_T *)ucRxBuffer);
                break;   
            case SHELL_MSG_PING:
                shell_do_ping((SM_PING_T *)ucRxBuffer);
                break;  
            case SHELL_MSG_BOOT:
                shell_do_boot((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;  
            case SHELL_MSG_DOWNLOAD_FILE:
                shell_do_download((SM_GENERIC_URL_T *)ucRxBuffer);
                break; 
            case SHELL_MSG_VERIFY_FILE:
                shell_do_verify((SM_GENERIC_URL_T *)ucRxBuffer);
                break; 
            case SHELL_MSG_BASIC_FILE:
                shell_do_basic_file((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;    
            case SHELL_MSG_BASIC_SCRIPT:
                shell_do_basic_script((SM_GENERIC_FILE_T *)ucRxBuffer);
                break; 
            case SHELL_MSG_BASIC_INTERACTIVE:
                shell_do_basic_interactive((SM_GENERIC_FILE_T *)ucRxBuffer);
                break;                                                                                                                                                          
            default:
                // Unknown or corrupted packet ID handler
                break;
            }
        }

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params); 
    }
}
