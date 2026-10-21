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

int shell_queue_list(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_LIST;

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
 * \brief display a list of files from the file system
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_list(SM_GENERIC_FILE_T *msg)
{
    if (!msg)
    {
        // error no filename
        return(-1);
    }

    picofs_list_all_files_from_cache(msg->filename);
    //picofs_list_all_files_from_flash(true);  // all files in flash including corrupted ones
    
    return(0);
}

int shell_queue_list_all(char *filename)
{
    SM_GENERIC_FILE_T msg;

    msg.message_id = SHELL_MSG_LIST_ALL;

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
 * \brief display a list of files from the file system including deleted / corrupted file remnants
 *
 * \param[in]  msg containing filename
 * \return nothing
 */
int shell_do_list_all(SM_GENERIC_FILE_T *msg)
{
    if (!msg)
    {
        // error no filename
        return(-1);
    }

    picofs_list_all_files_from_flash(msg->filename, true);
    
    return(0);
}