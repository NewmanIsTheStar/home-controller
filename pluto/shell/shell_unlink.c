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

int shell_queue_unlink(char *filename)
{
    SM_GENERIC_FILE_T msg;

    if (filename)
    {
        msg.message_id = SHELL_MSG_UNLINK;
        STRNCPY(msg.filename, filename, sizeof(msg.filename));
        xMessageBufferSend(xMessageBuffer, &msg, sizeof(msg), portMAX_DELAY);
    }

    return(0);
}

/*!
 * \brief print the contents of a text file to the shell
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_unlink(SM_GENERIC_FILE_T *msg)
{
    int err = -1;

    if (!msg || !(msg->filename[0]))
    {
        // error no filename
        shell_printf("rm: error: no filename provided\n");
        return(-1);
    }

    if (err = remove(msg->filename))
    {
        shell_printf("rm: failed to delete %s\n", msg->filename);
    }

    return(err);
}
