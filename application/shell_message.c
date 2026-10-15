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




MessageBufferHandle_t xMessageBuffer;

void vBufferInit(void) 
{
    // Create the message buffer
    xMessageBuffer = xMessageBufferCreate(STREAM_BUFFER_SIZE_BYTES);
    
    if (xMessageBuffer == NULL) 
    {
        // Source allocation failed (out of FreeRTOS heap)
    }
}

void vSenderTask(void *pvParameters) 
{
    // SensorData_t xSensor = { .xMsgId = MSG_ID_SENSOR_DATA, .fTemperature = 23.8 };
    // SetPoints_t xTarget  = { .xMsgId = MSG_ID_SETPOINTS,   .fTargetTemp = 25.0 };

    // xMessageBufferSend(xMessageBuffer, &xSensor, sizeof(xSensor), portMAX_DELAY);
    // xMessageBufferSend(xMessageBuffer, &xTarget, sizeof(xTarget), portMAX_DELAY);
}


#define MAX_MSG_SIZE   (32)

void shell_task(void *params) 
{
    uint8_t ucRxBuffer[MAX_MSG_SIZE];
    size_t xReceivedBytes;

    for (;;) 
    {
        xReceivedBytes = xMessageBufferReceive(xMessageBuffer, 
                                               ucRxBuffer, 
                                               MAX_MSG_SIZE, 
                                               portMAX_DELAY);

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

                
            default:
                // Unknown or corrupted packet ID handler
                break;
            }
        }

        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params); 
    }
}
