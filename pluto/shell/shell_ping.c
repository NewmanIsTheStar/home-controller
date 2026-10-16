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

int shell_queue_ping(char *address)
{
    SM_PING_T msg;

    msg.message_id = SHELL_MSG_PING;
    STRNCPY(msg.address, address, sizeof(msg.address));
    xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);

    return(0);
}

/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_ping(SM_PING_T *msg)
{
    int err = -1;

    if (!msg || !(msg->address[0]))
    {
        // error no filename
        shell_printf("ping: error: no address provided\n");
        return(-1);
    }

    shell_ping(msg->address);

    return(err);
}
