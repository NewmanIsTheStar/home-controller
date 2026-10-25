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

    if (dst && src)
    {    
        msg.message_id = SHELL_MSG_COPY;
        STRNCPY(msg.dst, dst, sizeof(msg.dst));
        STRNCPY(msg.src, src, sizeof(msg.src));

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

/*!
 * \brief copy a file
 *
 * \param[in]  old_name  file source
 * \param[in]  new_name  file destination
 * \return nothing
 */
int shell_copy(char *src_space_dst)
{
    FILE *filePointer;
    char buffer[256];
    int i, j;
    char src[16];
    char dst[16];

    src[0] = 0;
    dst[0] = 0;

    // extract source
    for(i=0; i<strlen(src_space_dst); i++)
    {
        if (src_space_dst[i] == ' ')
        {
            src[i++] = 0;
            break;
        }

        src[i] = src_space_dst[i];
        src[i+1] = 0;
    }

    // extract destination
    j = 0;
    for(; i<strlen(src_space_dst); i++)
    {
        if ((src_space_dst[i] == 0) || (!isprint(src_space_dst[i])))
        {
            dst[j] = 0;
            break;
        }

        dst[j++] = src_space_dst[i];
        dst[j] = 0;
    }

    if ((strlen(src) > 2) && (strlen(dst) > 2))
    {
        //picofs_copy(dst, src);
        shell_queue_copy(dst, src);
    }

    return 0;
}
