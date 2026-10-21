/*
 * Copyright (c) 2026 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "FreeRTOS.h"
#include "message_buffer.h"
#include "shell_message.h"
#include "shell.h"
#include "picofs.h"
extern MessageBufferHandle_t xMessageBuffer;

int shell_queue_stat(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_STAT;
    
    if (filename)
    {
        STRNCPY(msg.filename, filename, sizeof(msg.filename));
    }
    else
    {
        msg.filename[0] = 0;
    }
    
    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}


/*!
 * \brief display a details of a file
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_stat(SM_GENERIC_FILE_T *msg)
{
    if (!msg || !(msg->filename[0]))
    {
        // error no filename
        return(-1);
    }

    picofs_stat(msg->filename);
    
    return(0);
}
                   
