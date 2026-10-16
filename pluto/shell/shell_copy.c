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

int shell_queue_copy(char *dst, char *src)
{
    SM_GENERIC_DST_SRC_T msg;

    msg.message_id = SHELL_MSG_COPY;
    STRNCPY(msg.dst, dst, sizeof(msg.dst));
    STRNCPY(msg.src, src, sizeof(msg.src));

    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}


/*!
 * \brief display a details of a file
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_copy(SM_GENERIC_DST_SRC_T *msg)
{
    if (!msg || !(msg->dst[0]) || !(msg->src[0]))
    {
        // error no filename
        return(-1);
    }

    picofs_copy(msg->dst, msg->src);
    
    return(0);
}
                   
